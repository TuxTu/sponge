#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity) {
	s.resize(capacity);
	fitted.resize(capacity);
	head = 0;
	unassembled = 0;
	for(size_t i = 0; i < capacity; i++){
		fitted[i] = 'u';
	}
}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
	// If index is out of current capacity, just return.
	if(index > head + _capacity - 1)
		return;

	if(head >= index && index + data.size() >= head){
		// n bytes have been written to ByteStream
		size_t n = _output.write(data.substr(head - index));
		size_t new_head = head + n;
		while(head < new_head){
			// mark cache to be unfitted
			if(fitted[head%_capacity] != 'u'){
				unassembled--;
				fitted[head%_capacity] = 'u';
				s[head%_capacity] = 0;
			}
			head++;
		}
		// If there are substrings waitting to be assembled, write them.
		while(fitted[head%_capacity] != 'u'){
			string temp{};
			temp.resize(1);
			temp[0] = s[head%_capacity];
			if(_output.write(temp) == 0)
				return;
			if(fitted[head%_capacity] == 'e')
				_output.end_input();
			unassembled--;
			fitted[head%_capacity] = 'u';
			s[head++%_capacity] = 0;
		}
		if(eof)
			_output.end_input();
		return;
	}

	size_t i = index, j = 0;
	while(i < head + _capacity && j < data.size()){
		if(fitted[i%_capacity] == 'u'){
			fitted[i%_capacity] = 'f';
			s[i%_capacity] = data[j];
			unassembled++;
		}
		i++;
		j++;
	}
	// If eof, mark the end of the temp byte stream is -1.
	if(eof)
		fitted[--i%_capacity] = 'e';
	return;
}

size_t StreamReassembler::unassembled_bytes() const { return unassembled; }

bool StreamReassembler::empty() const { return unassembled == 0; }
