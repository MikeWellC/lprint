//
// Dymo driver for LPrint, a Label Printer Application
//
// Copyright © 2019-2020 by Michael R Sweet.
// Copyright © 2007-2019 by Apple Inc.
// Copyright © 2001-2007 by Easy Software Products.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

//
// Include necessary headers...
//

#include "lprint.h"


//
// Local types...
//

typedef struct lprint_dymo_s		// DYMO driver data
{
  unsigned	ystart,			// First line
		yend;			// Last line
  int		feed;			// Accumulated feed
} lprint_dymo_t;


//
// Local globals...
//

static const char * const lprint_dymo_media[] =
{					// Supported media sizes
  "oe_thin-multipurpose-label_0.375x2.8125in",
  "oe_library-barcode-label_0.5x1.875in",
  "oe_hanging-file-tab-insert_0.5625x2in",
  "oe_file-folder-label_0.5625x3.4375in",
  "oe_return-address-label_0.75x2in",
  "oe_barcode-label_0.75x2.5in",
  "oe_video-spine-label_0.75x5.875in",
  "oe_price-tag-label_0.9375x0.875in",
  "oe_square-multipurpose-label_1x1in",
  "oe_book-spine-label_1x1.5in",
  "oe_sm-multipurpose-label_1x2.125in",
  "oe_2-up-file-folder-label_1.125x3.4375in",
  "oe_internet-postage-label_1.25x1.625in",
  "oe_address-label_1.25x3.5in",
  "oe_lg-address-label_1.4x3.5in",
  "oe_video-top-label_1.8x3.1in",
  "oe_multipurpose-label_2x2.3125in",
  "oe_md-appointment-card_2x3.5in",
  "oe_lg-multipurpose-label_2.125x.75in",
  "oe_shipping-label_2.125x4in",
  "oe_continuous-label_2.125x3600in",
  "oe_md-multipurpose-label_2.25x1.25in",
  "oe_media-label_2.25x2.25in",
  "oe_2-up-address-label_2.25x3.5in",
  "oe_name-badge-label_2.25x4in",
  "oe_3-part-postage-label_2.25x7in",
  "oe_2-part-internet-postage-label_2.25x7.5in",
  "oe_shipping-label_2.3125x4in",
  "oe_internet-postage-label_2.3125x7in",
  "oe_internet-postage-confirmation-label_2.3125x10.5in",

  "roll_max_2.3125x3600in",
  "roll_min_0.25x0.25in"
};


//
// Local functions...
//

static int	lprint_dymo_print(lprint_job_t *job, lprint_options_t *options);
static int	lprint_dymo_rendjob(lprint_job_t *job, lprint_options_t *options);
static int	lprint_dymo_rendpage(lprint_job_t *job, lprint_options_t *options, unsigned page);
static int	lprint_dymo_rstartjob(lprint_job_t *job, lprint_options_t *options);
static int	lprint_dymo_rstartpage(lprint_job_t *job, lprint_options_t *options, unsigned page);
static int	lprint_dymo_rwrite(lprint_job_t *job, lprint_options_t *options, unsigned y, const unsigned char *line);
static int	lprint_dymo_status(lprint_printer_t *printer);


//
// 'lprintInitDymo()' - Initialize the driver.
//

void
lprintInitDYMO(
    lprint_driver_t *driver)		// I - Driver
{
  int	i;				// Looping var


  pthread_rwlock_wrlock(&driver->rwlock);

  driver->print      = lprint_dymo_print;
  driver->rendjob    = lprint_dymo_rendjob;
  driver->rendpage   = lprint_dymo_rendpage;
  driver->rstartjob  = lprint_dymo_rstartjob;
  driver->rstartpage = lprint_dymo_rstartpage;
  driver->rwrite     = lprint_dymo_rwrite;
  driver->status     = lprint_dymo_status;
  driver->format     = "application/vnd.dymo-lw";

  driver->num_resolution  = 1;
  driver->x_resolution[0] = 300;
  driver->y_resolution[0] = 300;

  driver->left_right = 100;
  driver->bottom_top = 525;

  driver->num_media = (int)(sizeof(lprint_dymo_media) / sizeof(lprint_dymo_media[0]));
  memcpy(driver->media, lprint_dymo_media, sizeof(lprint_dymo_media));

  if (strstr(driver->name, "-duo") || strstr(driver->name, "-twin"))
  {
    driver->num_source = 2;
    driver->source[0]  = "main-roll";
    driver->source[1]  = "alternate-roll";

    strlcpy(driver->media_ready[0].size_name, "oe_multipurpose-label_2x2.3125in", sizeof(driver->media_ready[0].size_name));
    strlcpy(driver->media_ready[1].size_name, "oe_address-label_1.25x3.5in", sizeof(driver->media_ready[1].size_name));
  }
  else
  {
    driver->num_source = 1;
    driver->source[0]  = "main-roll";
    strlcpy(driver->media_ready[0].size_name, "oe_address-label_1.25x3.5in", sizeof(driver->media_ready[0].size_name));
  }

  driver->tracking_supported = LPRINT_MEDIA_TRACKING_WEB;

  driver->num_type = 1;
  driver->type[0]  = "labels";

  driver->media_default.bottom_margin = driver->bottom_top;
  driver->media_default.left_margin   = driver->left_right;
  driver->media_default.right_margin  = driver->left_right;
  driver->media_default.size_width    = 3175;
  driver->media_default.size_length   = 8890;
  driver->media_default.top_margin    = driver->bottom_top;
  driver->media_default.tracking      = LPRINT_MEDIA_TRACKING_WEB;
  strlcpy(driver->media_default.size_name, "oe_address-label_1.25x3.5in", sizeof(driver->media_default.size_name));
  strlcpy(driver->media_default.source, driver->source[0], sizeof(driver->media_default.source));
  strlcpy(driver->media_default.type, driver->type[0], sizeof(driver->media_default.type));

  for (i = 0; i < driver->num_source; i ++)
  {
    pwg_media_t *pwg = pwgMediaForPWG(driver->media_ready[i].size_name);

    driver->media_ready[i].bottom_margin = driver->bottom_top;
    driver->media_ready[i].left_margin   = driver->left_right;
    driver->media_ready[i].right_margin  = driver->left_right;
    driver->media_ready[i].size_width    = pwg->width;
    driver->media_ready[i].size_length   = pwg->length;
    driver->media_ready[i].top_margin    = driver->bottom_top;
    driver->media_ready[i].tracking      = LPRINT_MEDIA_TRACKING_WEB;
    strlcpy(driver->media_ready[i].source, driver->source[i], sizeof(driver->media_ready[i].source));
    strlcpy(driver->media_ready[i].type, driver->type[0], sizeof(driver->media_ready[i].type));
  }

  driver->darkness_configured = 50;
  driver->darkness_supported  = 4;

  driver->num_supply = 0;

  pthread_rwlock_unlock(&driver->rwlock);
}


//
// 'lprint_dymo_print()' - Print a file.
//

static int				// O - 1 on success, 0 on failure
lprint_dymo_print(
    lprint_job_t     *job,		// I - Job
    lprint_options_t *options)		// I - Job options
{
  lprint_device_t *device = job->printer->driver->device;
					// Output device
  int		infd;			// Input file
  ssize_t	bytes;			// Bytes read/written
  char		buffer[65536];		// Read/write buffer


  // Reset the printer...
  lprintPutsDevice(device, "\033\033\033\033\033\033\033\033\033\033"
			   "\033\033\033\033\033\033\033\033\033\033"
			   "\033\033\033\033\033\033\033\033\033\033"
			   "\033\033\033\033\033\033\033\033\033\033"
			   "\033\033\033\033\033\033\033\033\033\033"
			   "\033\033\033\033\033\033\033\033\033\033"
			   "\033\033\033\033\033\033\033\033\033\033"
			   "\033\033\033\033\033\033\033\033\033\033"
			   "\033\033\033\033\033\033\033\033\033\033"
			   "\033\033\033\033\033\033\033\033\033\033"
			   "\033@");

  // Copy the raw file...
  job->impressions = 1;

  infd  = open(job->filename, O_RDONLY);

  while ((bytes = read(infd, buffer, sizeof(buffer))) > 0)
  {
    if (lprintWriteDevice(device, buffer, (size_t)bytes) < 0)
    {
      lprintLogJob(job, LPRINT_LOGLEVEL_ERROR, "Unable to send %d bytes to printer.", (int)bytes);
      close(infd);
      return (0);
    }
  }
  close(infd);

  job->impcompleted = 1;

  return (1);
}


//
// 'lprint_dymo_rend()' - End a job.
//

static int				// O - 1 on success, 0 on failure
lprint_dymo_rendjob(
    lprint_job_t     *job,		// I - Job
    lprint_options_t *options)		// I - Job options
{
  lprint_driver_t	*driver = job->printer->driver;
					// Driver data


  (void)options;

  free(driver->job_data);
  driver->job_data = NULL;

  return (1);
}


//
// 'lprint_dymo_rendpage()' - End a page.
//

static int				// O - 1 on success, 0 on failure
lprint_dymo_rendpage(
    lprint_job_t     *job,		// I - Job
    lprint_options_t *options,		// I - Job options
    unsigned         page)		// I - Page number
{
  lprint_device_t	*device = job->printer->driver->device;
					// Output device


  (void)options;
  (void)page;

  lprintPutsDevice(device, "\033E");

  return (1);
}


//
// 'lprint_dymo_rstartjob()' - Start a job.
//

static int				// O - 1 on success, 0 on failure
lprint_dymo_rstartjob(
    lprint_job_t     *job,		// I - Job
    lprint_options_t *options)		// I - Job options
{
  lprint_device_t	*device = job->printer->driver->device;
					// Output device
  lprint_dymo_t		*dymo = (lprint_dymo_t *)calloc(1, sizeof(lprint_dymo_t));
					// DYMO driver data


  (void)options;

  job->printer->driver->job_data = dymo;

  lprintPutsDevice(device, "\033\033\033\033\033\033\033\033\033\033"
			   "\033\033\033\033\033\033\033\033\033\033"
			   "\033\033\033\033\033\033\033\033\033\033"
			   "\033\033\033\033\033\033\033\033\033\033"
			   "\033\033\033\033\033\033\033\033\033\033"
			   "\033\033\033\033\033\033\033\033\033\033"
			   "\033\033\033\033\033\033\033\033\033\033"
			   "\033\033\033\033\033\033\033\033\033\033"
			   "\033\033\033\033\033\033\033\033\033\033"
			   "\033\033\033\033\033\033\033\033\033\033"
			   "\033@");

  return (1);
}


//
// 'lprint_dymo_rstartpage()' - Start a page.
//

static int				// O - 1 on success, 0 on failure
lprint_dymo_rstartpage(
    lprint_job_t     *job,		// I - Job
    lprint_options_t *options,		// I - Job options
    unsigned         page)		// I - Page number
{
  lprint_driver_t	*driver = job->printer->driver;
					// Driver
  lprint_device_t	*device = job->printer->driver->device;
					// Output device
  lprint_dymo_t		*dymo = (lprint_dymo_t *)job->printer->driver->job_data;
					// DYMO driver data
  int			darkness = job->printer->driver->darkness_configured + options->print_darkness;
  const char		*density = "cdeg";
					// Density codes


  (void)page;

  if (options->header.cupsBytesPerLine > 256)
  {
    lprintLogJob(job, LPRINT_LOGLEVEL_ERROR, "Raster data too large for printer.");
    return (0);
  }

  lprintPrintfDevice(device, "\033Q%c%c", 0, 0);
  lprintPrintfDevice(device, "\033B%c", 0);
  lprintPrintfDevice(device, "\033L%c%c", options->header.cupsHeight >> 8, options->header.cupsHeight);
  lprintPrintfDevice(device, "\033D%c", options->header.cupsBytesPerLine - 1);
  lprintPrintfDevice(device, "\033q%d", !strcmp(options->media.source, "alternate-roll") ? 2 : 1);

  if (darkness < 0)
    darkness = 0;
  else if (darkness > 100)
    darkness = 100;

  lprintPrintfDevice(device, "\033%c", density[3 * darkness / 100]);

  dymo->feed   = 0;
  dymo->ystart = driver->bottom_top * options->printer_resolution[1] / 2540;
  dymo->yend   = options->header.cupsHeight - dymo->ystart;

  return (1);
}


//
// 'lprint_dymo_rwrite()' - Write a raster line.
//

static int				// O - 1 on success, 0 on failure
lprint_dymo_rwrite(
    lprint_job_t        *job,		// I - Job
    lprint_options_t    *options,	// I - Job options
    unsigned            y,		// I - Line number
    const unsigned char *line)		// I - Line
{
  lprint_device_t	*device = job->printer->driver->device;
					// Output device
  lprint_dymo_t		*dymo = (lprint_dymo_t *)job->printer->driver->job_data;
					// DYMO driver data
  unsigned char		buffer[256];	// Write buffer


  if (y < dymo->ystart || y >= dymo->yend)
    return (1);

  if (line[0] || memcmp(line, line + 1, options->header.cupsBytesPerLine - 1))
  {
    // Not a blank line, feed for any prior blank lines...
    if (dymo->feed)
    {
      while (dymo->feed > 255)
      {
	lprintPrintfDevice(device, "\033f\001%c", 255);
	dymo->feed -= 255;
      }

      lprintPrintfDevice(device, "\033f\001%c", dymo->feed);
      dymo->feed = 0;
    }

    // Then write the non-blank line...
    buffer[0] = 0x16;
    memcpy(buffer + 1, line + 1, options->header.cupsBytesPerLine - 1);
    lprintWriteDevice(device, buffer, options->header.cupsBytesPerLine);
  }
  else
  {
    // Blank line, accumulate the feed...
    dymo->feed ++;
  }

  return (1);
}


//
// 'lprint_dymo_status()' - Get current printer status.
//

static int				// O - 1 on success, 0 on failure
lprint_dymo_status(
    lprint_printer_t *printer)		// I - Printer
{
  (void)printer;

  return (1);
}
