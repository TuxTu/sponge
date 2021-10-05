#include "stream_reassembler.hh"
#include <iostream>
// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity) {
	_head = 0;
	_unassembled = 0;
}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
	// If index is out of current capacity, just return.
	if(index >= _head + _capacity)
		return;

	// If data could be write immediately, then write it
	if(_head >= index && index + data.size() >= _head) {
		bool end{false};
		size_t n = min(_capacity, data.size() - _head + index);
		end = eof && (n == data.size() - _head + index);
		std::string s = data.substr(_head - index, n);

		set<Substring>::iterator ss;
		for(ss = _substrings.begin(); ss != _substrings.end() && not _substrings.empty();) {
			if(ss->begin <= _head + n) {
				if(ss->begin + ss->s.size() > _head + n) {
					s += ss->s.substr(_head + n - ss->begin);
					n = s.size();
					end |= ss->eof;
				}
				_unassembled -= ss->s.size();
				ss = _substrings.erase(ss);
				continue;
			}
			++ss;
		}
		n = _output.write(s);
		_head += n;
		if(n != s.size()) {
			Substring temp{};
			temp.begin = _head;
			temp.s = s.substr(n);
			temp.eof = end;
			_substrings.insert(temp);
			_unassembled += temp.s.size();
			return;
		}else if(end)
			_output.end_input();
		return;
	}

	set<Substring>::iterator ss;
	Substring temp{};
	size_t n = min(_capacity - index + _head, data.size());
	temp.begin = index;
	temp.s = data.substr(0, n);
	temp.eof = eof && (n == data.size());
	for(ss = _substrings.begin(); ss != _substrings.end() && not _substrings.empty();) {
		if(not (ss->begin + ss->s.size() < temp.begin || temp.begin + temp.s.size() < ss->begin)) {
			if(ss->begin <= temp.begin) {
				if(ss->begin + ss->s.size() > temp.begin + temp.s.size()) {
					temp.s += ss->s.substr(temp.begin + temp.s.size() - ss->begin);
				}
				temp.s = ss->s.substr(0, temp.begin - ss->begin) + temp.s;
				temp.begin = ss->begin;
			} else {
				if(ss->begin + ss->s.size() > temp.begin + temp.s.size()) {
					temp.s += ss->s.substr(temp.begin + temp.s.size() - ss->begin);
				}
			}
			_unassembled -= ss->s.size();
			temp.eof |= ss->eof;
			ss = _substrings.erase(ss);
			continue;
		}
		++ss;
	}

	_substrings.insert(temp);
	_unassembled += temp.s.size();

	return;
}

	/*
	// If index is out of current capacity, just return.
	if(index >= head + _capacity)
		return;

	if(head >= index && index + data.size() > head){
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
		if(eof && index + data.size() == head)
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
	if(eof && j == data.size())
		fitted[--i%_capacity] = 'e';
	return;
}
*/

size_t StreamReassembler::assembled_bytes() const { return _head; }

size_t StreamReassembler::unassembled_bytes() const { return _unassembled; }

bool StreamReassembler::empty() const { return _unassembled == 0; }
