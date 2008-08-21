/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   class definition for the TTA output module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __P_TTA_H
#define __P_TTA_H

#include "os.h"

#include "common.h"
#include "pr_generic.h"

class tta_packetizer_c: public generic_packetizer_c {
private:
  int channels, bits_per_sample, sample_rate;
  int64_t samples_output;

public:
  tta_packetizer_c(generic_reader_c *_reader, int _channels,
                   int _bits_per_sample, int _sample_rate, track_info_c &_ti)
    throw (error_c);
  virtual ~tta_packetizer_c();

  virtual int process(packet_cptr packet);
  virtual void set_headers();

  virtual const char *get_format_name() {
    return "TTA";
  }
  virtual connection_result_e can_connect_to(generic_packetizer_c *src,
                                             string &error_message);
};

#endif // __P_TTA_H
