/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL v2
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   PGS packetizer

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <matroska/KaxContentEncoding.h>
#include <matroska/KaxTracks.h>

#include "common/codec.h"
#include "common/compression.h"
#include "common/pgssup.h"
#include "output/p_hdmv_pgs.h"

using namespace libmatroska;

hdmv_pgs_packetizer_c::hdmv_pgs_packetizer_c(generic_reader_c *p_reader,
                                   track_info_c &p_ti)
  : generic_packetizer_c(p_reader, p_ti)
  , m_aggregate_packets(false)
{
  set_track_type(track_subtitle);
  set_default_compression_method(COMPRESSION_ZLIB);
}

hdmv_pgs_packetizer_c::~hdmv_pgs_packetizer_c() {
}

void
hdmv_pgs_packetizer_c::set_headers() {
  set_codec_id(MKV_S_HDMV_PGS);

  generic_packetizer_c::set_headers();

  m_track_entry->EnableLacing(false);
}

int
hdmv_pgs_packetizer_c::process(packet_cptr packet) {
  if (!m_aggregate_packets) {
    add_packet(packet);
    return FILE_STATUS_MOREDATA;
  }

  if (!m_aggregated) {
    m_aggregated = packet;
    m_aggregated->data->take_ownership();

  } else
    m_aggregated->data->add(packet->data);

  if (   (0                                != packet->data->get_size())
      && (mtx::pgs::END_OF_DISPLAY_SEGMENT == packet->data->get_buffer()[0])) {
    add_packet(m_aggregated);
    m_aggregated.reset();
  }

  return FILE_STATUS_MOREDATA;
}

connection_result_e
hdmv_pgs_packetizer_c::can_connect_to(generic_packetizer_c *src,
                                 std::string &) {
  return !dynamic_cast<hdmv_pgs_packetizer_c *>(src) ? CAN_CONNECT_NO_FORMAT : CAN_CONNECT_YES;
}
