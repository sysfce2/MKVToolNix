/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   class definition for the DTS output module

   Written by Peter Niemayer <niemayer@isg.de>.
   Modified by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __P_DTS_H
#define __P_DTS_H

#include "os.h"

#include "common.h"
#include "pr_generic.h"
#include "dts_common.h"

class dts_packetizer_c: public generic_packetizer_c {
private:
  int64_t samples_written, bytes_written;

  unsigned char *packet_buffer;
  int buffer_size;

  bool get_first_header_later;
  dts_header_t first_header;
  dts_header_t last_header;

public:
  bool skipping_is_normal;

  dts_packetizer_c(generic_reader_c *_reader, const dts_header_t &dts_header,
                   track_info_c &_ti, bool _get_first_header_later = false)
    throw (error_c);
  virtual ~dts_packetizer_c();

  virtual int process(packet_cptr packet);
  virtual void set_headers();
  virtual const char *get_format_name() {
    return "DTS";
  }
  virtual connection_result_e can_connect_to(generic_packetizer_c *src,
                                             string &error_message);

private:
  virtual void add_to_buffer(unsigned char *buf, int size);
  virtual unsigned char *get_dts_packet(dts_header_t &dts_header);
  virtual int dts_packet_available();
  virtual void remove_dts_packet(int pos, int framesize);
};

#endif // __P_DTS_H
