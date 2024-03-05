#include "byte_stream.hh"
#include <cstring>

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : capacity_( capacity ) { 
  buffer_.reserve( capacity );
}

bool Writer::is_closed() const
{
  return this->is_closed_;
}

void Writer::push( string data )
{
  size_t available_capacity = this->available_capacity();
  size_t bytes_to_copy = min(data.size(), available_capacity);

  buffer_ += data.substr(0, bytes_to_copy);
  bytes_pushed_ += bytes_to_copy;
}

void Writer::close()
{
  is_closed_ = true;
}

uint64_t Writer::available_capacity() const
{
  return capacity_ - buffer_.size();
}

uint64_t Writer::bytes_pushed() const
{
  return bytes_pushed_;
}

bool Reader::is_finished() const
{
  return ((is_closed_ == true) && (this->bytes_buffered() == 0));
}

uint64_t Reader::bytes_popped() const
{
  return bytes_popped_;
}

string_view Reader::peek() const
{
  string_view sv( buffer_.data() , min(static_cast<uint64_t>(1000),this->bytes_buffered()));
  return sv;
}

void Reader::pop( uint64_t len )
{
  buffer_.erase( buffer_.begin() , buffer_.begin() + len );
  bytes_popped_ += len;
}

uint64_t Reader::bytes_buffered() const
{
  return buffer_.size();
}
