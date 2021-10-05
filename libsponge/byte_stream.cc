#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`


using namespace std;

ByteStream::ByteStream(const size_t capacity) : _bsize(capacity), _left_space(capacity) {
	_w_bytes = 0;
	_r_bytes = 0;
	_ended = 0;
}

size_t ByteStream::write(const string &data) {
	size_t len = min(data.length(), _left_space);
	
	_buffer.append(move(string().assign(data.begin(), data.begin()+len)));
	_w_bytes += len;
	_left_space -= len;

	return len;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
	std::string s = _buffer.concatenate();
	size_t length = min(len, _bsize - _left_space);

    return string().assign(s.begin(), s.begin() + length);
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
	size_t length = min(len, _bsize - _left_space);
	_buffer.remove_prefix(length);
	_left_space += length;
	_r_bytes += length;
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
	std::string s = peek_output(len);
	pop_output(len);

	return s;
}

void ByteStream::end_input() {
	_ended = true;
}

bool ByteStream::input_ended() const { return _ended == true; }

size_t ByteStream::buffer_size() const { return _bsize-_left_space; }

bool ByteStream::buffer_empty() const { 
	return _left_space == _bsize; 
}

bool ByteStream::eof() const { return (_ended == true) && (_left_space == _bsize); }

size_t ByteStream::bytes_written() const { return _w_bytes; }

size_t ByteStream::bytes_read() const { return _r_bytes; }

size_t ByteStream::remaining_capacity() const { return _left_space; }
