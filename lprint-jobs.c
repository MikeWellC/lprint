//
// Jobs sub-command for LPrint, a Label Printer Application
//
// Copyright © 2019-2020 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

//
// Include necessary headers...
//

#include "lprint.h"


//
// 'lprintDoJobs()' - Do the jobs sub-command.
//

int					// O - Exit status
lprintDoJobs(int           num_options,	// I - Number of options
	     cups_option_t *options)	// I - Options
{
  const char	*printer_uri,		// Printer URI
		*printer_name;		// Printer name
  char		default_printer[256],	// Default printer
		resource[1024];		// Resource path
  http_t	*http;			// Server connection
  ipp_t		*request,		// IPP request
		*response;		// IPP response
  ipp_attribute_t *attr;		// Current attribute
  const char	*attrname;		// Attribute name
  int		job_id,			// Current job-id
		job_state;		// Current job-state
  const char	*job_name,		// Current job-name
		*job_user;		// Current job-originating-user-name


  if ((printer_uri = cupsGetOption("printer-uri", num_options, options)) != NULL)
  {
    // Connect to the remote printer...
    if ((http = lprintConnectURI(printer_uri, resource, sizeof(resource))) == NULL)
      return (1);
  }
  else
  {
    // Connect to/start up the server and get the destination printer...
    http = lprintConnect(1);

    if ((printer_name = cupsGetOption("printer-name", num_options, options)) == NULL)
    {
      if ((printer_name = lprintGetDefaultPrinter(http, default_printer, sizeof(default_printer))) == NULL)
      {
	fputs("lprint: No default printer available.\n", stderr);
	httpClose(http);
	return (1);
      }
    }
  }

  // Send a Get-Jobs request...
  request = ippNewRequest(IPP_OP_GET_JOBS);
  if (printer_uri)
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL, printer_uri);
  else
    lprintAddPrinterURI(request, printer_name, resource, sizeof(resource));
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsUser());
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD, "which-joba", NULL, "all");

  response = cupsDoRequest(http, request, resource);

  for (attr = ippFirstAttribute(response); attr; attr = ippNextAttribute(response))
  {
    if (ippGetGroupTag(attr) == IPP_TAG_OPERATION)
      continue;

    job_id    = 0;
    job_state = IPP_JSTATE_PENDING;
    job_name  = "(none)";
    job_user  = "(unknown)";

    while (ippGetGroupTag(attr) == IPP_TAG_JOB)
    {
      attrname = ippGetName(attr);
      if (!strcmp(attrname, "job-id"))
        job_id = ippGetInteger(attr, 0);
      else if (!strcmp(attrname, "job-name"))
        job_name = ippGetString(attr, 0, NULL);
      else if (!strcmp(attrname, "job-originating-user-name"))
        job_user = ippGetString(attr, 0, NULL);
      else if (!strcmp(attrname, "job-state"))
        job_state = ippGetInteger(attr, 0);

      attr = ippNextAttribute(response);
    }

    printf("%d %-12s %-16s %s\n", job_id, ippEnumString("job-state", job_state), job_user, job_name);
  }

  ippDelete(response);
  httpClose(http);

  return (0);
}
