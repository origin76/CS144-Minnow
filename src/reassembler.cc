#include "reassembler.hh"
#include<iostream>

using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{
  cout << first_index << " " << data.size() << endl;
  uint64_t last_index = first_index + data.size();
  uint64_t first_unacceptable = first_unassembled_index_ + output_.writer().available_capacity();
  if ( is_last_substring ) {
    final_index_ = last_index;
  }
  if ( first_index < first_unassembled_index_ ) {
    if ( first_index + data.size() <= first_unassembled_index_ ) {
      check_push();
      return;
    } else {
      data.erase( 0, first_unassembled_index_ - first_index );
      first_index = first_unassembled_index_;
    }
  }
  if ( first_unacceptable <= first_index ) {
    return;
  }
  if ( last_index > first_unacceptable ) {
    data.erase( first_unacceptable - first_index );
  }
  if ( !segments_.empty() ) {
    auto cur = segments_.lower_bound( Seg( first_index, "" ) );
    if ( cur != segments_.begin() ) {
      cur--;
      if ( cur->first_index + cur->data.size() > first_index ) {
        data.erase( 0, cur->first_index + cur->data.size() - first_index );
        first_index += cur->first_index + cur->data.size() - first_index;
      }
    }

    cur = segments_.lower_bound( Seg( first_index, "" ) );
    while ( cur != segments_.end() && cur->first_index < last_index ) {
      if ( cur->first_index + cur->data.size() <= last_index ) {
        bytes_waiting_ -= cur->data.size();
        segments_.erase( cur );
        cur = segments_.lower_bound( Seg( first_index, "" ) );
      } else {
        data.erase( cur->first_index - first_index);
        break;
      }
    }
  }

  segments_.insert( Seg( first_index, data ) );
  bytes_waiting_ += data.size();

  cout << first_index << " " << data.size() << endl;
  cout << "bytes_waiting: " << bytes_waiting_ << endl;
  
  check_push();
}

uint64_t Reassembler::bytes_pending() const
{
  return bytes_waiting_;
}

void Reassembler::check_push() {
  while ( !segments_.empty() ) {
    auto seg = segments_.begin(); 
    if ( seg->first_index == first_unassembled_index_ ) {
      output_.writer().push( seg->data );
      auto tmp = seg;
      first_unassembled_index_ += seg->data.size();
      bytes_waiting_ -= seg->data.size();
      segments_.erase(tmp);
      if ( first_unassembled_index_ >= final_index_ ) {
        output_.writer().close();
      }
    } else {
      break;
    }
  }
  cout << "first_unassembled_index_: " << first_unassembled_index_ << endl;
} 