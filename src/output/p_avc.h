/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   class definition for the MPEG 4 part 10 ES video output module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __P_AVC_H
#define __P_AVC_H

#include "os.h"

#include "common.h"
#include "mpeg4_common.h"
#include "pr_generic.h"

using namespace mpeg4::p10;

class mpeg4_p10_es_video_packetizer_c: public generic_packetizer_c {
protected:
  avc_es_parser_c m_parser;
  memory_cptr m_avcc;
  int m_width, m_height;
  bool m_allow_timecode_generation, m_first_frame;

public:
  mpeg4_p10_es_video_packetizer_c(generic_reader_c *n_reader,
                                  memory_cptr avcc, int width, int height,
                                  track_info_c &n_ti);

  virtual int process(packet_cptr packet);
  virtual void set_headers();

  virtual void flush();
  virtual void flush_frames();

  virtual void enable_timecode_generation(bool enable,
                                          int64_t default_duration = -1);
  virtual void extract_aspect_ratio();

  virtual const char *get_format_name() {
    return "video";
  };
  virtual connection_result_e can_connect_to(generic_packetizer_c *src,
                                             string &error_message);
};

#endif // __P_AVC_H
