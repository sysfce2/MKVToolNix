/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL v2
   see the file COPYING for details
   or visit https://www.gnu.org/licenses/old-licenses/gpl-2.0.html

   WAVPACK demultiplexer module

   Written by Steve Lhomme <steve.lhomme@free.fr>.
   Modified by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include "common/codec.h"
#include "common/endian.h"
#include "common/error.h"
#include "common/id_info.h"
#include "common/mm_file_io.h"
#include "input/r_wavpack.h"
#include "merge/input_x.h"
#include "merge/file_status.h"
#include "output/p_wavpack.h"

bool
wavpack_reader_c::probe_file() {
  if (m_in->read(&header, sizeof(mtx::wavpack::header_t)) != sizeof(mtx::wavpack::header_t))
    return false;

  if (memcmp(header.ck_id, "wvpk", 4) != 0)
    return false;

  auto version = get_uint16_le(&header.version);
  return (version >> 8) == 4;
}

void
wavpack_reader_c::read_headers() {
  try {
    int packet_size = mtx::wavpack::parse_frame(*m_in, header, meta, true, true);
    if (0 > packet_size)
      mxerror_fn(m_ti.m_fname, Y("The file header was not read correctly.\n"));
  } catch (...) {
    throw mtx::input::open_x();
  }

  m_in->setFilePointer(m_in->getFilePointer() - sizeof(mtx::wavpack::header_t));

  // correction file if applies
  meta.has_correction = false;

  try {
    if (header.flags & mtx::wavpack::HYBRID_FLAG) {
      m_in_correc = mm_file_io_c::open(m_ti.m_fname + "c");
      int packet_size = mtx::wavpack::parse_frame(*m_in_correc, header_correc, meta_correc, true, true);
      if (0 > packet_size)
        mxerror_fn(m_ti.m_fname, Y("The correction file header was not read correctly.\n"));

      m_in_correc->setFilePointer(m_in_correc->getFilePointer() - sizeof(mtx::wavpack::header_t));
      meta.has_correction = true;
    }
  } catch (...) {
    if (verbose)
      mxinfo_fn(m_ti.m_fname, fmt::format(FY("Could not open the corresponding correction file '{0}c'.\n"), m_ti.m_fname));
  }

  if (!verbose)
    return;

  show_demuxer_info();
  if (meta.has_correction)
    mxinfo_fn(m_ti.m_fname, fmt::format(FY("Also using the correction file '{0}c'.\n"), m_ti.m_fname));
}

void
wavpack_reader_c::create_packetizer(int64_t) {
  if (!demuxing_requested('a', 0) || !m_reader_packetizers.empty())
    return;

  m_ti.m_private_data = memory_c::alloc(sizeof(uint16_t));
  put_uint16_le(m_ti.m_private_data->get_buffer(), header.version);
  add_packetizer(new wavpack_packetizer_c(this, m_ti, meta));

  show_packetizer_info(0, ptzr(0));
}

file_status_e
wavpack_reader_c::read(generic_packetizer_c *,
                       bool) {
  mtx::wavpack::header_t dummy_header, dummy_header_correc;
  mtx::wavpack::meta_t dummy_meta;
  uint64_t initial_position = m_in->getFilePointer();
  uint8_t *chunk, *databuffer;

  // determine the final data size
  int32_t data_size = 0, block_size, truncate_bytes;
  int extra_frames_number = -1;

  dummy_meta.channel_count = 0;
  while (dummy_meta.channel_count < meta.channel_count) {
    extra_frames_number++;
    block_size = mtx::wavpack::parse_frame(*m_in, dummy_header, dummy_meta, false, false);
    if (-1 == block_size)
      return flush_packetizers();
    data_size += block_size;
    m_in->skip(block_size);
  }

  if (0 > data_size)
    return flush_packetizers();

  data_size += 3 * sizeof(uint32_t);
  if (extra_frames_number)
    data_size += sizeof(uint32_t) + extra_frames_number * 3 * sizeof(uint32_t);
  chunk = (uint8_t *)safemalloc(data_size);

  // keep the header minus the ID & size (all found in Matroska)
  put_uint32_le(chunk, dummy_header.block_samples);

  m_in->setFilePointer(initial_position);

  dummy_meta.channel_count = 0;
  databuffer               = &chunk[4];
  while (dummy_meta.channel_count < meta.channel_count) {
    block_size = mtx::wavpack::parse_frame(*m_in, dummy_header, dummy_meta, false, false);
    put_uint32_le(databuffer, dummy_header.flags & ~mtx::wavpack::HAS_CHECKSUM);
    databuffer += 4;
    put_uint32_le(databuffer, dummy_header.crc);
    databuffer += 4;
    if (2 < meta.channel_count) {
      // not stored for the last block
      put_uint32_le(databuffer, block_size);
      databuffer += 4;
    }
    if (m_in->read(databuffer, block_size) != static_cast<size_t>(block_size))
      return flush_packetizers();

    // If the WavPack block contains a trailing checksum (added for WavPack 5) then delete it here because it's
    // useless without the included 32-byte WavPack header. This also applies to the correction block below.

    truncate_bytes = (dummy_header.flags & mtx::wavpack::HAS_CHECKSUM) ? mtx::wavpack::checksum_byte_count (databuffer, block_size) : 0;
    if (2 < meta.channel_count)
      put_uint32_le(databuffer - 4, block_size - truncate_bytes);
    databuffer += block_size - truncate_bytes;
    data_size  -= truncate_bytes;
  }

  packet_cptr packet(new packet_t(memory_c::take_ownership(chunk, data_size)));

  // find the if there is a correction file data corresponding
  if (!m_in_correc) {
    ptzr(0).process(packet);
    return FILE_STATUS_MOREDATA;
  }

  do {
    initial_position         = m_in_correc->getFilePointer();
    // determine the final data size
    data_size                = 0;
    extra_frames_number      = 0;
    dummy_meta.channel_count = 0;

    while (dummy_meta.channel_count < meta_correc.channel_count) {
      extra_frames_number++;
      block_size = mtx::wavpack::parse_frame(*m_in_correc, dummy_header_correc, dummy_meta, false, false);
      if (-1 == block_size)
        return flush_packetizers();
      data_size += block_size;
      m_in_correc->skip(block_size);
    }

    // no more correction to be found
    if (0 > data_size) {
      m_in_correc.reset();
      dummy_header_correc.block_samples = dummy_header.block_samples + 1;
      break;
    }
  } while (dummy_header_correc.block_samples < dummy_header.block_samples);

  if (dummy_header_correc.block_samples != dummy_header.block_samples) {
    ptzr(0).process(packet);
    return FILE_STATUS_MOREDATA;
  }

  m_in_correc->setFilePointer(initial_position);

  if (meta.channel_count > 2)
    data_size += extra_frames_number * 2 * sizeof(uint32_t);
  else
    data_size += sizeof(uint32_t);

  auto mem                 = memory_c::alloc(data_size);
  auto chunk_correc        = mem->get_buffer();
  // only keep the CRC in the header
  dummy_meta.channel_count = 0;
  databuffer               = chunk_correc;

  while (dummy_meta.channel_count < meta_correc.channel_count) {
    block_size = mtx::wavpack::parse_frame(*m_in_correc, dummy_header_correc, dummy_meta, false, false);
    put_uint32_le(databuffer, dummy_header_correc.crc);
    databuffer += 4;
    if (2 < meta_correc.channel_count) {
      // not stored for the last block
      put_uint32_le(databuffer, block_size);
      databuffer += 4;
    }
    if (m_in_correc->read(databuffer, block_size) != static_cast<size_t>(block_size))
      m_in_correc.reset();
    truncate_bytes = (dummy_header_correc.flags & mtx::wavpack::HAS_CHECKSUM) ? mtx::wavpack::checksum_byte_count (databuffer, block_size) : 0;
    if (2 < meta_correc.channel_count)
      put_uint32_le(databuffer - 4, block_size - truncate_bytes);
    databuffer += block_size - truncate_bytes;
    data_size  -= truncate_bytes;
  }

  mem->resize(data_size);
  packet->data_adds.push_back(mem);

  ptzr(0).process(packet);

  return FILE_STATUS_MOREDATA;
}

void
wavpack_reader_c::identify() {
  auto info = mtx::id::info_c{};

  info.add(mtx::id::audio_channels,           meta.channel_count);
  info.add(mtx::id::audio_sampling_frequency, meta.sample_rate);
  if (meta.bits_per_sample)
    info.add(mtx::id::audio_bits_per_sample,  meta.bits_per_sample);

  id_result_container();
  id_result_track(0, ID_RESULT_TRACK_AUDIO, codec_c::get_name(codec_c::type_e::A_WAVPACK4, "WAVPACK"), info.get());
}
