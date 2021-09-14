#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`


using namespace std;

ByteStream::ByteStream(const size_t capacity) : bsize(capacity), left_space(capacity) {
	buffer.resize(capacity);
	w_head = 0;
	r_head = 0;
	w_bytes = 0;
	r_bytes = 0;
	ended = 0;
}

size_t ByteStream::write(const string &data) {
	int i = 0;
	
	while(data[i] != 0 && left_space > 0){
		buffer[w_head] = data[i];
		w_head = (w_head + 1) % bsize;
		w_bytes++;
		i++;
		left_space--;
	}

	return i;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
	string s;
	s.resize(len);
	size_t rh = r_head;
	size_t n = ended ? min(bsize-left_space+1, len) : min(bsize-left_space, len);
	
	for(size_t i = 0; i < n; i++){
		s[i] = buffer[rh];
		rh = (rh + 1) % bsize;
	}
    return s;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
	size_t n = ended ? min(bsize-left_space+1, len) : min(bsize-left_space, len);
	left_space += n;
	r_bytes += n;
	r_head = (r_head + n) % bsize;
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
	std::string s;
	s.resize(len);
	size_t n = ended ? min(bsize-left_space+1, len) : min(bsize-left_space, len);

	for(size_t i = 0; i < n; i++){
		s[i] = buffer[r_head];
		r_bytes++;
		left_space++;
		r_head = (r_head + 1) % bsize;
	}

	return s;
}

void ByteStream::end_input() {
	ended = true;
	buffer[w_head++] = EOF;
}

bool ByteStream::input_ended() const { return ended == true; }

size_t ByteStream::buffer_size() const { return bsize-left_space; }

bool ByteStream::buffer_empty() const { 
	return left_space == bsize; 
}

bool ByteStream::eof() const { return (ended == true) && (left_space == bsize); }

size_t ByteStream::bytes_written() const { return w_bytes; }

size_t ByteStream::bytes_read() const { return r_bytes; }

size_t ByteStream::remaining_capacity() const { return left_space; }
