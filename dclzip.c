/*
 * PKDCLX 1.1.2
 * Copyright (c) 2026 Jeffrey H. Johnson <johnsonjh.dev@gmail.com>
 * SPDX-License-Identifier: MIT-0
 */

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pkdcl.h"

#if defined(_WIN32) || defined(__MSDOS__) || defined(MSDOS)
# include <fcntl.h>
# include <io.h>
#endif

#define TOOL_NAME "dclzip"
#define IO_BUFSIZE 32768U

struct io_ctx
{
  FILE *in;
  FILE *out;
  unsigned long in_size;
  unsigned long out_size;
  unsigned long crc;
  int crc_on_read;
  int crc_on_write;
  int read_error;
  int write_error;
};

struct options
{
  int decompress;
  int to_stdout;
  int verbose;
  int force;
  unsigned short mode;
  unsigned short dict;
  unsigned long xdict;
  int extra;
  int optimal;
};

static void
usage (FILE *fp)
{
  (void)fputs (
      "Compress (or decompress) PKWARE DCL Implode (method 10) streams.\n\n",
      fp);
  (void)fprintf (fp, "Usage: %s [ option(s) ] [ filenames(s) ]\n", TOOL_NAME);
  (void)fputs ("\nOptions:\n", fp);
  (void)fputs ("  -d, --decompress  decompress (default: compress)\n", fp);
  (void)fputs ("  -c, --stdout      write result to standard output\n", fp);
  (void)fputs (
      "  -v, --verbose     verbose status (report CRC, sizes, and ratio)\n",
      fp);
  (void)fputs ("  -f, --force       overwrite existing output file\n", fp);
  (void)fputs ("      --ascii       ASCII-literal compression mode\n", fp);
  (void)fputs (
      "      --binary      binary-literal compression mode (default)\n", fp);
  (void)fputs ("      --1k          1024-byte dictionary\n", fp);
  (void)fputs ("      --2k          2048-byte dictionary\n", fp);
  (void)fputs ("      --4k          4096-byte dictionary (default)\n", fp);
  (void)fputs ("      --8k          8192-byte dictionary (extended)\n", fp);
  (void)fputs ("      --16k         16384-byte dictionary (extended)\n", fp);
  (void)fputs ("      --32k         32768-byte dictionary (extended)\n", fp);
  (void)fputs ("      --extra       tighter (but slower) parsing\n", fp);
  (void)fputs ("      --optimal     optimal (but slowest) parsing\n", fp);
  (void)fputs ("  -h, --help        display this help\n\n", fp);
  (void)fputs (
      "If no filenames(s) specified, data is read from standard input.\n", fp);
}

static void
set_binary (FILE *fp)
{
#if defined(_WIN32) || defined(__MSDOS__) || defined(MSDOS)
  (void)_setmode (_fileno (fp), O_BINARY);
#else
  (void)fp;
#endif
}

static void
crc_update (struct io_ctx *ctx, const unsigned char *buf, unsigned short n)
{
  unsigned short count;
  unsigned long old;

  count = n;
  old = ctx->crc;
  ctx->crc = pkdcl_crc32 (buf, &count, &old);
}

static unsigned short
/* cppcheck-suppress constParameterCallback */
read_cb (unsigned char *buffer, unsigned short *size, void *opaque)
{
  struct io_ctx *ctx;
  size_t want;
  size_t got;

  ctx = (struct io_ctx *)opaque;
  want = (size_t)*size;
  got = fread (buffer, 1, want, ctx->in);

  if (got != 0)
    {
      ctx->in_size += (unsigned long)got;

      if (ctx->crc_on_read)
        {
          crc_update (ctx, buffer, (unsigned short)got);
        }
    }

  if (got < want && ferror (ctx->in))
    {
      ctx->read_error = 1;
    }

  return (unsigned short)got;
}

static void
/* cppcheck-suppress constParameterCallback */
write_cb (unsigned char *buffer, unsigned short *size, void *opaque)
{
  struct io_ctx *ctx;
  size_t want;
  size_t put;

  ctx = (struct io_ctx *)opaque;
  want = (size_t)*size;

  if (ctx->write_error)
    {
      return;
    }

  put = fwrite (buffer, 1, want, ctx->out);

  if (put != 0)
    {
      ctx->out_size += (unsigned long)put;

      if (ctx->crc_on_write)
        {
          crc_update (ctx, buffer, (unsigned short)put);
        }
    }

  if (put != want)
    {
      ctx->write_error = 1;
    }
}

static int
exists_file (const char *name)
{
  FILE *fp;

  fp = fopen (name, "rb");

  if (fp == NULL)
    {
      return 0;
    }

  (void)fclose (fp);

  return 1;
}

static char *
make_output_name (const char *name, int decompress, int extended)
{
  size_t n;
  char *out;

  n = strlen (name);

  if (decompress && n >= 5 && strcmp (name + n - 5, ".dclx") == 0)
    {
      if (n == 5)
        {
          return NULL;
        }

      out = (char *)malloc (n - 4);

      if (out == NULL)
        {
          return NULL;
        }

      (void)memcpy (out, name, n - 5);
      out[n - 5] = '\0';

      return out;
    }

  if (decompress && n >= 4 && strcmp (name + n - 4, ".dcl") == 0)
    {
      if (n == 4)
        {
          return NULL;
        }

      out = (char *)malloc (n - 3);

      if (out == NULL)
        {
          return NULL;
        }

      (void)memcpy (out, name, n - 4);
      out[n - 4] = '\0';

      return out;
    }

  if (decompress)
    {
      out = (char *)malloc (n + 5);

      if (out == NULL)
        {
          return NULL;
        }

      (void)memcpy (out, name, n);
      (void)memcpy (out + n, ".out", 5);

      return out;
    }

  if (extended)
    {
      out = (char *)malloc (n + 6);

      if (out == NULL)
        {
          return NULL;
        }

      (void)memcpy (out, name, n);
      (void)memcpy (out + n, ".dclx", 6);
    }
  else
    {
      out = (char *)malloc (n + 5);

      if (out == NULL)
        {
          return NULL;
        }

      (void)memcpy (out, name, n);
      (void)memcpy (out + n, ".dcl", 5);
    }

  return out;
}

static void
report_stats (const char *name, const struct io_ctx *ctx, int decompress)
{
  unsigned long plain;
  unsigned long packed;
  double ratio;
  unsigned long final_crc;

  if (decompress)
    {
      packed = ctx->in_size;
      plain = ctx->out_size;
    }
  else
    {
      plain = ctx->in_size;
      packed = ctx->out_size;
    }

  if (plain == 0UL)
    {
      ratio = 0.0;
    }
  else
    {
      ratio = 100.0 * ((double)plain - (double)packed) / (double)plain;
    }

  final_crc = ctx->crc ^ 0xffffffffUL;
  (void)fprintf (stderr,
                 "%s: CRC=%08lX  input=%lu  output=%lu  ratio=%.1f%%\n", name,
                 final_crc & 0xffffffffUL, ctx->in_size, ctx->out_size, ratio);
}

static const char *
dcl_error (unsigned short r)
{
  switch (r)
    {
    case PKDCL_CMP_NO_ERROR:
      return "no error";

    case PKDCL_CMP_INVALID_DICTSIZE:
      return "invalid dictionary size";

    case PKDCL_CMP_INVALID_MODE:
      return "invalid compression mode";

    case PKDCL_CMP_BAD_DATA:
      return "invalid or truncated DCL stream";

    case PKDCL_CMP_ABORT:
      return "DCL operation aborted";

    default:
      return "unknown DCL error";
    }
}

static int
process_one (const char *name, const struct options *opt)
{
  struct io_ctx ctx;
  FILE *in;
  FILE *out;
  char *outname;
  unsigned char *work;
  unsigned short r;
  unsigned short mode;
  unsigned short dict;
  int stdin_input;
  int stdout_output;
  int status;

  stdin_input = strcmp (name, "-") == 0;
  stdout_output = opt->to_stdout || stdin_input;
  outname = NULL;
  status = 1;

  if (stdin_input)
    {
      in = stdin;
      set_binary (in);
    }
  else
    {
      in = fopen (name, "rb");

      if (in == NULL)
        {
          (void)fprintf (stderr, "%s: %s: %s\n", TOOL_NAME, name,
                         strerror (errno));

          return 1;
        }
    }

  if (stdout_output)
    {
      out = stdout;
      set_binary (out);
    }
  else
    {
      outname = make_output_name (name, opt->decompress, opt->xdict > 4096UL);

      if (outname == NULL)
        {
          (void)fprintf (stderr, "%s: cannot construct output name for %s\n",
                         TOOL_NAME, name);

          if (!stdin_input) /* //-V547 */
            {
              (void)fclose (in);
            }

          return 1;
        }

      if (!opt->force && exists_file (outname))
        {
          (void)fprintf (stderr,
                         "%s: %s already exists; use -f to overwrite\n",
                         TOOL_NAME, outname);
          free (outname);

          if (!stdin_input) /* //-V547 */
            {
              (void)fclose (in);
            }

          return 1;
        }

      out = fopen (outname, "wb");

      if (out == NULL)
        {
          (void)fprintf (stderr, "%s: %s: %s\n", TOOL_NAME, outname,
                         strerror (errno));
          free (outname);

          if (!stdin_input) /* //-V547 */
            {
              (void)fclose (in);
            }

          return 1;
        }
    }

  (void)memset (&ctx, 0, sizeof (ctx));
  ctx.in = in;
  ctx.out = out;
  ctx.crc = 0xffffffffUL;
  ctx.crc_on_read = !opt->decompress;
  ctx.crc_on_write = opt->decompress;

  if (opt->decompress)
    {
      work = (unsigned char *)malloc (PKDCL_EXPLODE_WORK_SIZE);
    }
  else
    {
      work = (unsigned char *)malloc (PKDCL_IMPLODE_WORK_SIZE);
    }

  if (work == NULL)
    {
      (void)fprintf (stderr, "%s: out of memory\n", TOOL_NAME);

      goto done;
    }

  if (opt->decompress)
    {
      r = pkdcl_explode_ex (read_cb, write_cb, &ctx);
    }
  else
    {
      mode = opt->mode;
      dict = opt->dict;

      if (opt->optimal || opt->extra || opt->xdict > 4096UL)
        {
          unsigned int flags = 0U;

          if (opt->optimal)
            {
              flags = PKDCL_FLAG_OPTIMAL;
            }
          else if (opt->extra)
            {
              flags = PKDCL_FLAG_EXTRA;
            }

          r = pkdcl_implode_ex (read_cb, write_cb, &ctx, mode, opt->xdict,
                                flags);
        }
      else
        {
          r = pkdcl_implode (read_cb, write_cb, work, &ctx, &mode, &dict);
        }
    }

  free (work);

  if (ctx.read_error)
    {
      (void)fprintf (stderr, "%s: %s: input error\n", TOOL_NAME, name);

      goto done;
    }

  if (ctx.write_error || fflush (out) == EOF)
    {
      (void)fprintf (stderr, "%s: output error: %s\n", TOOL_NAME,
                     strerror (errno));

      goto done;
    }

  if (r != PKDCL_CMP_NO_ERROR)
    {
      (void)fprintf (stderr, "%s: %s: %s (DCL status %u)\n", TOOL_NAME, name,
                     dcl_error (r), (unsigned)r);

      goto done;
    }

  if (opt->verbose)
    {
      report_stats (name, &ctx, opt->decompress);
    }

  status = 0;

done:
  if (!stdout_output)
    {
      if (fclose (out) == EOF && status == 0)
        {
          (void)fprintf (stderr, "%s: %s: close error\n",
          /* cppcheck-suppress incorrectStringBooleanError */
                         TOOL_NAME ? TOOL_NAME : "unknown",
                         outname ? outname : "unknown");
          status = 1;
        }

      if (status != 0)
        {
          if (outname)
            {
              (void)remove (outname);
            }
        }
    }

  if (!stdin_input)
    {
      (void)fclose (in);
    }

  free (outname);

  return status;
}

static int
long_option (const char *arg, struct options *opt)
{
  if (strcmp (arg, "--decompress") == 0)
    {
      opt->decompress = 1;
    }
  else if (strcmp (arg, "--stdout") == 0)
    {
      opt->to_stdout = 1;
    }
  else if (strcmp (arg, "--verbose") == 0)
    {
      opt->verbose = 1;
    }
  else if (strcmp (arg, "--force") == 0)
    {
      opt->force = 1;
    }
  else if (strcmp (arg, "--ascii") == 0)
    {
      opt->mode = PKDCL_CMP_ASCII;
    }
  else if (strcmp (arg, "--binary") == 0)
    {
      opt->mode = PKDCL_CMP_BINARY;
    }
  else if (strcmp (arg, "--1k") == 0)
    {
      opt->dict = 1024U;
      opt->xdict = 1024UL;
    }
  else if (strcmp (arg, "--2k") == 0)
    {
      opt->dict = 2048U;
      opt->xdict = 2048UL;
    }
  else if (strcmp (arg, "--4k") == 0)
    {
      opt->dict = 4096U;
      opt->xdict = 4096UL;
    }
  else if (strcmp (arg, "--8k") == 0)
    {
      opt->xdict = 8192UL;
    }
  else if (strcmp (arg, "--16k") == 0)
    {
      opt->xdict = 16384UL;
    }
  else if (strcmp (arg, "--32k") == 0)
    {
      opt->xdict = 32768UL;
    }
  else if (strcmp (arg, "--extra") == 0)
    {
      opt->extra = 1;
    }
  else if (strcmp (arg, "--optimal") == 0)
    {
      opt->optimal = 1;
    }
  else if (strcmp (arg, "--help") == 0)
    {
      usage (stdout);

      exit (0);
    }
  else
    {
      return 0;
    }

  return 1;
}

static int
short_options (const char *arg, struct options *opt)
{
  const char *p;

  p = arg + 1;

  while (*p != '\0')
    {
      switch (*p++)
        {
        case 'd':
          opt->decompress = 1;

          break;

        case 'c':
          opt->to_stdout = 1;

          break;

        case 'v':
          opt->verbose = 1;

          break;

        case 'f':
          opt->force = 1;

          break;

        case 'h':
          usage (stdout);

          exit (0);

        default:
          return 0;
        }
    }

  return 1;
}

int
main (int argc, char **argv)
{
  struct options opt;
  int i;
  int first_file;
  int files;
  int status;
  int end_options;

  (void)memset (&opt, 0, sizeof (opt));
  opt.mode = PKDCL_CMP_BINARY;
  opt.dict = 4096U;
  opt.xdict = 4096UL;
  first_file = argc;
  files = 0;
  end_options = 0;

  for (i = 1; i < argc; i++)
    {
      if (!end_options && strcmp (argv[i], "--") == 0)
        {
          end_options = 1;

          continue;
        }

      if (!end_options && argv[i][0] == '-' && argv[i][1] != '\0')
        {
          if (argv[i][1] == '-')
            {
              if (!long_option (argv[i], &opt))
                {
                  (void)fprintf (stderr, "%s: unknown option: %s\n", TOOL_NAME,
                                 argv[i]);

                  return 2;
                }
            }
          else if (!short_options (argv[i], &opt))
            {
              (void)fprintf (stderr, "%s: unknown option: %s\n", TOOL_NAME,
                             argv[i]);

              return 2;
            }
        }
      else
        {
          if (first_file == argc)
            {
              first_file = i;
            }

          files++;
        }
    }

  if (files != 0)
    {
      for (i = first_file; i < argc; i++)
        {
          if (argv[i][0] == '-' && strcmp (argv[i], "-") != 0 && !end_options)
            {
              (void)fprintf (stderr,
                             "%s: options must precede file operands\n",
                             TOOL_NAME);

              return 2;
            }
        }
    }

  if (files == 0)
    {
      return process_one ("-", &opt);
    }

  if (opt.to_stdout && files > 1)
    {
      (void)fprintf (stderr,
                     "%s: raw DCL streams cannot be safely concatenated; "
                     "use -c with one input\n",
                     TOOL_NAME);

      return 2;
    }

  status = 0;
  end_options = 0;

  for (i = 1; i < argc; i++)
    {
      if (!end_options && strcmp (argv[i], "--") == 0)
        {
          end_options = 1;

          continue;
        }

      if (!end_options && argv[i][0] == '-' && argv[i][1] != '\0')
        {
          continue;
        }

      if (process_one (argv[i], &opt) != 0)
        {
          status = 1;
        }
    }

  return status;
}
