/*
 *  rmff.c
 *
 *  Copyright (C) Moritz Bunkus - March 2004
 *      
 *  librmff is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation; either version 2.1, or (at your option)
 *  any later version.
 *   
 *  librmff is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *   
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this library; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA. 
 *
 */

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "librmff.h"

int rmff_last_error = RMFF_ERR_OK;
const char *rmff_last_error_msg = NULL;
static const char *rmff_std_error_messages[] = {
  "No error",
  "File is not a RealMedia file",
  "Inconsistent data found in file",
  "End of file reached",
  "Input/output error",
  "Invalid parameters"
};

const char *
rmff_get_error_str(int code) {
  code *= -1;
  if ((code >= 0) && (code <= 5))
    return rmff_std_error_messages[code];
  else
    return "Unknown error";
}

static int
set_error(int error_number,
          const char *error_msg,
          int return_value) {
  rmff_last_error = error_number;
  if (error_msg == NULL)
    rmff_last_error_msg = rmff_get_error_str(error_number);
  else
    rmff_last_error_msg = error_msg;

  return return_value;
}

static uint16_t
get_uint16_be(const void *buf) {
  uint16_t ret;
  unsigned char *tmp;

  tmp = (unsigned char *) buf;

  ret = tmp[0] & 0xff;
  ret = (ret << 8) + (tmp[1] & 0xff);

  return ret;
}

static uint32_t
get_uint32_be(const void *buf) {
  uint32_t ret;
  unsigned char *tmp;

  tmp = (unsigned char *) buf;

  ret = tmp[0] & 0xff;
  ret = (ret << 8) + (tmp[1] & 0xff);
  ret = (ret << 8) + (tmp[2] & 0xff);
  ret = (ret << 8) + (tmp[3] & 0xff);

  return ret;
}

static void
put_uint16_be(void *buf,
              uint16_t value) {
  unsigned char *tmp;

  tmp = (unsigned char *) buf;

  tmp[1] = value & 0xff;
  tmp[0] = (value >>= 8) & 0xff;
}

static void
put_uint32_be(void *buf,
              uint32_t value) {
  unsigned char *tmp;

  tmp = (unsigned char *) buf;

  tmp[3] = value & 0xff;
  tmp[2] = (value >>= 8) & 0xff;
  tmp[1] = (value >>= 8) & 0xff;
  tmp[0] = (value >>= 8) & 0xff;
}

#define read_uint8() file_read_uint8(io, fh)
#define read_uint16_be() file_read_uint16_be(io, fh)
#define read_uint32_be() file_read_uint32_be(io, fh)
#define read_uint16_be_to(addr) put_uint16_be(addr, read_uint16_be())
#define read_uint32_be_to(addr) put_uint32_be(addr, read_uint32_be())
#define get_fourcc(b) get_uint32_be(b)

static uint8_t
file_read_uint8(mb_file_io_t *io,
                void *fh) {
  unsigned char tmp;

  io->read(fh, &tmp, 1);
  return tmp;
}

static uint16_t
file_read_uint16_be(mb_file_io_t *io,
                    void *fh) {
  unsigned char tmp[2];

  io->read(fh, tmp, 2);
  return get_uint16_be(tmp);
}

static uint32_t
file_read_uint32_be(mb_file_io_t *io,
                    void *fh) {
  unsigned char tmp[4];

  io->read(fh, tmp, 4);
  return get_uint32_be(tmp);
}

void
die(const char *fmt,
    ...) {
  va_list ap;

  fprintf(stderr, "'die' called: ");
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fprintf(stderr, "\n");
  exit(1);
}

#define safestrdup(s) _safestrdup(s, __FILE__, __LINE__)
#define safememdup(s, b) _safememdup(s, b, __FILE__, __LINE__)
#define safemalloc(s) _safemalloc(s, __FILE__, __LINE__)
#define safecalloc(s) _safecalloc(s, __FILE__, __LINE__)
#define saferealloc(s, b) _saferealloc(s, b, __FILE__, __LINE__)
#define safefree(s) { if ((s) != NULL) free(s); }

static char *
_safestrdup(const char *s,
            const char *file,
            int line) {
  char *copy;

  if (s == NULL)
    return NULL;

  copy = strdup(s);
  if (copy == NULL)
    die("safestrdup() called from file %s, line %d: strdup() "
        "returned NULL for '%s'.", file, line, s);

  return copy;
}

/* static void * */
/* _safememdup(const void *s, */
/*             size_t size, */
/*             const char *file, */
/*             int line) { */
/*   void *copy; */

/*   if (s == NULL) */
/*     return NULL; */

/*   copy = malloc(size); */
/*   if (copy == NULL) */
/*     die("safememdup() called from file %s, line %d: malloc() " */
/*         "returned NULL for a size of %d bytes.", file, line, size); */
/*   memcpy(copy, s, size); */

/*   return copy; */
/* } */

static void *
_safemalloc(size_t size,
            const char *file,
            int line) {
  void *mem;

  mem = malloc(size);
  if (mem == NULL)
    die("safemalloc() called from file %s, line %d: malloc() "
        "returned NULL for a size of %d bytes.", file, line, size);

  return mem;
}

static void *_safecalloc(size_t size,
                         const char *file,
                         int line) {
  void *mem;

  mem = calloc(size, 1);
  if (mem == NULL)
    die("safemalloc() called from file %s, line %d: malloc() "
        "returned NULL for a size of %d bytes.", file, line, size);

  return mem;
}

static void *
_saferealloc(void *mem,
             size_t size,
             const char *file,
             int line) {
  mem = realloc(mem, size);
  if (mem == NULL)
    die("saferealloc() called from file %s, line %d: realloc() "
        "returned NULL for a size of %d bytes.", file, line, size);

  return mem;
}

rmff_file_t *
rmff_open_file(const char *path,
               int mode) {
  return rmff_open_file_with_io(path, mode, &std_mb_file_io);
}

rmff_file_t *
rmff_open_file_with_io(const char *path,
                       int mode,
                       mb_file_io_t *io) {
  rmff_file_t *file;
  void *file_h;
  char buf[5];

  if ((path == NULL) || (io == NULL) ||
      ((mode != RMFF_OPEN_MODE_READING) && (mode != RMFF_OPEN_MODE_WRITING)))
    return (rmff_file_t *)set_error(RMFF_ERR_PARAMETERS, NULL, 0);

  if (mode == RMFF_OPEN_MODE_READING)
    mode = MB_OPEN_MODE_READING;
  else
    mode = MB_OPEN_MODE_WRITING;
  file_h = io->open(path, mode);
  if (file_h == NULL)
    return NULL;

  buf[4] = 0;
  if ((io->read(file_h, buf, 4) != 4) ||
      strcmp(buf, ".RMF")) {
    io->close(file_h);
    return (rmff_file_t *)set_error(RMFF_ERR_NOT_RMFF, NULL, 0);
  }

  file = (rmff_file_t *)safecalloc(sizeof(rmff_file_t));
  file->handle = file_h;
  file->name = safestrdup(path);
  file->io = io;
  io->seek(file_h, 0, SEEK_END);
  file->size = io->tell(file_h);
  io->seek(file_h, 4, SEEK_SET);

  return file;
}

void
rmff_free_track_data(rmff_track_t *track) {
  if (track == NULL)
    return;
  safefree(track->mdpr_header.name);
  safefree(track->mdpr_header.mime_type);
  safefree(track->mdpr_header.type_specific_data);
}

void
rmff_close_file(rmff_file_t *file) {
  int i;

  if (file == NULL)
    return;

  safefree(file->name);
  for (i = 0; i < file->num_tracks; i++)
    rmff_free_track_data(&file->tracks[i]);
  safefree(file->tracks);
  safefree(file->cont_header.title);
  safefree(file->cont_header.author);
  safefree(file->cont_header.copyright);
  safefree(file->cont_header.comment);
  file->io->close(file->handle);
  safefree(file);
}


#define skip(num) { if (io->seek(fh, num, SEEK_CUR) != 0) \
                      return set_error(RMFF_ERR_IO, NULL, -1); }

int
rmff_read_headers(rmff_file_t *file) {
  mb_file_io_t *io;
  void *fh;
  uint32_t object_id, object_size, size;
  uint16_t object_version;
  rmff_prop_t *prop;
  rmff_cont_t *cont;
  rmff_mdpr_t *mdpr;
  rmff_track_t track;
  real_video_props_t *rvp;
  real_audio_v4_props_t *ra4p;

  if (file == NULL)
    return set_error(RMFF_ERR_PARAMETERS, NULL, RMFF_ERR_PARAMETERS);
  if (file->headers_read)
    return 0;

  io = file->io;
  fh = file->handle;
  if (io->seek(fh, 4, SEEK_SET))
    return set_error(RMFF_ERR_IO, NULL, -1);

  skip(4);                      /* header_size */
  skip(2);                      /* object_version */
  skip(4);                      /* file_version */
  skip(4);                      /* num_headers */

  prop = &file->prop_header;
  cont = &file->cont_header;
  while (1) {
    rmff_last_error = RMFF_ERR_OK;
    object_id = read_uint32_be();
    object_size = read_uint32_be();
    object_version = read_uint16_be();
    if (rmff_last_error != RMFF_ERR_OK)
      break;

    if (object_id == rmffFOURCC('P', 'R', 'O', 'P')) {
      read_uint32_be_to(&prop->max_bit_rate);
      read_uint32_be_to(&prop->avg_bit_rate);
      read_uint32_be_to(&prop->max_packet_size);
      read_uint32_be_to(&prop->avg_packet_size);
      read_uint32_be_to(&prop->num_packets);
      read_uint32_be_to(&prop->duration);
      read_uint32_be_to(&prop->preroll);
      read_uint32_be_to(&prop->index_offset);
      read_uint32_be_to(&prop->data_offset);
      read_uint16_be_to(&prop->num_streams);
      read_uint16_be_to(&prop->flags);
      file->prop_header_found = 1;

    } else if (object_id == rmffFOURCC('C', 'O', 'N', 'T')) {
      if (file->cont_header_found) {
        safefree(cont->title);
        safefree(cont->author);
        safefree(cont->copyright);
        safefree(cont->comment);
      }
      memset(cont, 0, sizeof(rmff_cont_t));

      size = read_uint16_be(); /* title_len */
      if (size > 0) {
        cont->title = (char *)safemalloc(size + 1);
        io->read(fh, cont->title, size);
      }
      size = read_uint16_be(); /* author_len */
      if (size > 0) {
        cont->author = (char *)safemalloc(size + 1);
        io->read(fh, cont->author, size);
      }
      size = read_uint16_be(); /* copyright_len */
      if (size > 0) {
        cont->copyright = (char *)safemalloc(size + 1);
        io->read(fh, cont->copyright, size);
      }
      size = read_uint16_be(); /* comment_len */
      if (size > 0) {
        cont->comment = (char *)safemalloc(size + 1);
        io->read(fh, cont->comment, size);
      }
      file->cont_header_found = 1;

    } else if (object_id == rmffFOURCC('M', 'D', 'P', 'R')) {
      memset(&track, 0, sizeof(rmff_track_t));
      track.file = (struct rmff_file_t *)file;
      mdpr = &track.mdpr_header;
      read_uint16_be_to(&mdpr->id);
      track.id = get_uint16_be(&mdpr->id);
      read_uint32_be_to(&mdpr->max_bit_rate);
      read_uint32_be_to(&mdpr->avg_bit_rate);
      read_uint32_be_to(&mdpr->max_packet_size);
      read_uint32_be_to(&mdpr->avg_packet_size);
      read_uint32_be_to(&mdpr->start_time);
      read_uint32_be_to(&mdpr->preroll);
      read_uint32_be_to(&mdpr->duration);
      size = read_uint8(); /* stream_name_len */
      if (size > 0) {
        mdpr->name = (char *)safemalloc(size + 1);
        io->read(fh, mdpr->name, size);
      }
      size = read_uint8(); /* mime_type_len */
      if (size > 0) {
        mdpr->mime_type = (char *)safemalloc(size + 1);
        io->read(fh, mdpr->mime_type, size);
      }
      size = read_uint32_be();  /* type_specific_size */
      mdpr->type_specific_size = size; 
      if (size > 0) {
        mdpr->type_specific_data = (unsigned char *)safemalloc(size);
        io->read(fh, mdpr->type_specific_data, size);
      }

      rvp = (real_video_props_t *)mdpr->type_specific_data;
      ra4p = (real_audio_v4_props_t *)mdpr->type_specific_data;
      if ((size >= sizeof(real_video_props_t)) &&
          (get_fourcc(&rvp->fourcc1) == rmffFOURCC('V', 'I', 'D', 'O')))
        track.type = RMFF_TRACK_TYPE_VIDEO;
      else if ((size >= sizeof(real_audio_v4_props_t)) &&
               (get_fourcc(&ra4p->fourcc1) ==
                rmffFOURCC('.', 'r', 'a', 0xfd))) {
        track.type = RMFF_TRACK_TYPE_AUDIO;
        if ((get_uint16_be(&ra4p->version1) == 5) &&
            (size < sizeof(real_audio_v5_props_t)))
          return set_error(RMFF_ERR_DATA, "RealAudio v5 data indicated but "
                           "data too small", RMFF_ERR_DATA);
      }

      file->tracks =
        (rmff_track_t *)saferealloc(file->tracks, (file->num_tracks + 1) *
                                    sizeof(rmff_track_t));
      file->tracks[file->num_tracks] = track;
      file->num_tracks++;

    } else if (object_id == rmffFOURCC('D', 'A', 'T', 'A')) {
      file->num_packets_in_chunk = read_uint32_be();
      file->next_data_header_offset = read_uint32_be();
      file->first_data_header_offset = io->tell(fh) - (4 + 4 + 2);
      break;

    } else {
      /* Unknown header type */
      return set_error(RMFF_ERR_DATA, NULL, RMFF_ERR_DATA);
    }
  }

  if (file->prop_header_found && (file->first_data_header_offset > 0)) {
    file->headers_read = 1;
    return 0;
  }
  return set_error(RMFF_ERR_DATA, NULL, RMFF_ERR_DATA);
}

int
rmff_get_next_frame_size(rmff_file_t *file) {
  uint16_t object_version, length;
  uint32_t object_id;
  mb_file_io_t *io;
  void *fh;
  int result;
  int64_t old_pos;

  if ((file == NULL) || (!file->headers_read) || (file->io == NULL) ||
      (file->handle == NULL))
    return set_error(RMFF_ERR_PARAMETERS, NULL, RMFF_ERR_PARAMETERS);
  io = file->io;
  fh = file->handle;
  old_pos = io->tell(fh);
  if ((file->size - old_pos) < 12)
    return set_error(RMFF_ERR_EOF, NULL, RMFF_ERR_EOF);

  object_version = read_uint16_be();
  length = read_uint16_be();
  object_id = ((uint32_t)object_version) << 16 | length;
  if (object_id == rmffFOURCC('D', 'A', 'T', 'A')) {
    skip(4 + 4);                /* packets_in_chunk, next_data_header */
    result = rmff_get_next_frame_size(file);
    io->seek(fh, old_pos, SEEK_SET);
    return result;
  }
  io->seek(fh, old_pos, SEEK_SET);
  if (object_id == rmffFOURCC('I', 'N', 'D', 'X'))
    return set_error(RMFF_ERR_EOF, NULL, RMFF_ERR_EOF);
  return length - 12;
}

rmff_frame_t *
rmff_read_next_frame(rmff_file_t *file,
                     void *buffer) {
  rmff_frame_t *frame;
  uint16_t object_version, length;
  uint32_t object_id;
  mb_file_io_t *io;
  void *fh;

  if ((file == NULL) || (!file->headers_read) || (file->io == NULL) ||
      (file->handle == NULL))
    return (rmff_frame_t *)set_error(RMFF_ERR_PARAMETERS, NULL, 0);
  io = file->io;
  fh = file->handle;
  if ((file->size - io->tell(fh)) < 12)
    return (rmff_frame_t *)set_error(RMFF_ERR_EOF, NULL, 0);

  object_version = read_uint16_be();
  length = read_uint16_be();
  object_id = ((uint32_t)object_version) << 16 | length;
  if (object_id == rmffFOURCC('D', 'A', 'T', 'A')) {
    file->num_packets_in_chunk = read_uint32_be();
    file->next_data_header_offset = read_uint32_be();
    file->num_packets_read = 0;
    return rmff_read_next_frame(file, buffer);
  }
  if ((file->num_packets_read >= file->num_packets_in_chunk) ||
      (object_id == rmffFOURCC('I', 'N', 'D', 'X')))
    return (rmff_frame_t *)set_error(RMFF_ERR_EOF, NULL, 0);

  frame = (rmff_frame_t *)safecalloc(sizeof(rmff_frame_t));
  if (buffer == NULL) {
    buffer = safemalloc(length);
    frame->allocated_by_rmff = 1;
  }
  frame->data = buffer;
  frame->size = length - 12;
  frame->id = read_uint16_be();
  frame->timecode = read_uint32_be();
  frame->reserved = read_uint8();
  frame->flags = read_uint8();
  if (io->read(fh, frame->data, frame->size) != frame->size) {
    rmff_release_frame(frame);
    return (rmff_frame_t *)set_error(RMFF_ERR_EOF, NULL, 0);
  }
  file->num_packets_read++;

  return frame;
}

void
rmff_release_frame(rmff_frame_t *frame) {
  if (frame == NULL)
    return;
  if (frame->allocated_by_rmff)
    safefree(frame->data);
  safefree(frame);
}
