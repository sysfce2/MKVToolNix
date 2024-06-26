/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL v2
   see the file COPYING for details
   or visit https://www.gnu.org/licenses/old-licenses/gpl-2.0.html

   Kate packetizer

   Written by ogg.k.ogg.k <ogg.k.ogg.k@googlemail.com>.
   Adapted from code by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <ogg/ogg.h>

#include "common/codec.h"
#include "common/endian.h"
#include "common/kate.h"
#include "merge/connection_checks.h"
#include "output/p_kate.h"

kate_packetizer_c::kate_packetizer_c(generic_reader_c *reader,
                                     track_info_c &ti)
  : generic_packetizer_c{reader, ti, track_subtitle}
  , m_previous_timestamp{}
{
  // the number of headers to expect is stored in the first header
  auto blocks = unlace_memory_xiph(m_ti.m_private_data);

  parse_identification_header(blocks[0]->get_buffer(), blocks[0]->get_size(), m_kate_id);
  if (blocks.size() != m_kate_id.nheaders)
    throw false;

  m_frame_duration = mtx::rational(m_kate_id.gden, m_kate_id.gnum) * 1'000'000'000;

  set_language(mtx::bcp47::language_c::parse(m_kate_id.language));
  int i;
  for (i = 0; i < m_kate_id.nheaders; ++i)
    m_headers.push_back(memory_cptr(blocks[i]->clone()));
}

kate_packetizer_c::~kate_packetizer_c() {
}

void
kate_packetizer_c::set_headers() {
  set_codec_id(MKV_S_KATE);
  set_codec_private(lace_memory_xiph(m_headers));

  generic_packetizer_c::set_headers();
}

void
kate_packetizer_c::process_impl(packet_cptr const &packet) {
  if (packet->data->get_size() < (1 + 3 * sizeof(int64_t))) {
    /* end packet is 1 byte long and has type 0x7f */
    if ((packet->data->get_size() == 1) && (packet->data->get_buffer()[0] == 0x7f)) {
      packet->timestamp = m_previous_timestamp;
      packet->duration  = 0;
      add_packet(packet);

    } else
      mxwarn_tid(m_ti.m_fname, m_ti.m_id, Y("Kate packet is too small and is being skipped.\n"));

    return;
  }

  int64_t start_time    = get_uint64_le(packet->data->get_buffer() + 1);
  int64_t duration      = get_uint64_le(packet->data->get_buffer() + 1 + sizeof(int64_t));

  packet->timestamp     = mtx::to_int(start_time * m_frame_duration);
  packet->duration      = mtx::to_int(duration   * m_frame_duration);
  packet->gap_following = true;

  int64_t end           = packet->timestamp + packet->duration;

  if (end > m_previous_timestamp)
    m_previous_timestamp = end;

  packet->force_key_frame();

  add_packet(packet);
}

connection_result_e
kate_packetizer_c::can_connect_to(generic_packetizer_c *src,
                                  std::string &error_message) {
  kate_packetizer_c *psrc = dynamic_cast<kate_packetizer_c *>(src);
  if (!psrc)
    return CAN_CONNECT_NO_FORMAT;

  connect_check_codec_private(src);

  return CAN_CONNECT_YES;
}
