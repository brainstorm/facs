#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <dirent.h>
#include <limits.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <time.h>
#include <libgen.h>
#include <sys/time.h>

#ifndef __clang__
#include <omp.h>
#endif

#include "tool.h"
#include "bloom.h"
#include "file_dir.h"

int
query_read(char *begin, int length, char model, bloom * bl, 
           float tole_rate, F_set * File_head)
{
  //printf("%s\n", begin);
  char *cur = begin;
  char *prev = begin;

  int len = length;
  int signal = 0, result = 0;
 
/* 
 * // Quick pass
 * *cur = begin;
 * *prev = begin;
 * int hits;
 *
 * bloom bl = NEW(bloom);
 * int len = length;
 * int end = begin + length - k_mer;
 *
 * while(cur < end)
 * {
 *   // Check from 5' 
 *   if(bloom_test(bl, cur, len))
 *
 *   cur+=k_mer;
 * }
 *
 * rc_begin = rev_trans(begin)
 * cur = rc_begin;
 *
 * while(cur < end) {
 *   // Check from 3' (rev)
 *
 * }
 *
 */


  while (len > bl->k_mer) {
      //printf("%d, %d, %s\n", len, bl->k_mer, cur);
      if (signal == 1)
	    break;

      if (len >= bl->k_mer) {
          prev = cur;
          cur += bl->k_mer;
          len -= bl->k_mer;
      } else {
          cur += len - bl->k_mer;
          signal = 1;
      }

      if (model == 'r')
        rev_trans (cur);

      if (bloom_check (bl, cur, bl->k_mer)) {
        result = read_full_check (bl, begin, length, model, tole_rate, File_head);

        if (result > 0)
            return result;
        else if (model == 'n')
            break;
      }
  }

  if (model == 'r')
    return 0;
  else
    return query_read(begin, length, 'r', bl, tole_rate, File_head);
}

int
read_full_check (bloom * bl, char *begin, int length, char model, float tole_rate, F_set * File_head)
{
  char *cur = begin;
  float result;
  int len = length;
  int count = 0, match_s = 0, mark = 1, match_time = 0;
  short prev = 0, conse = 0;

  while (length >= bl->k_mer) {

      if (model == 'r')
        rev_trans (cur);

      if (count >= bl->k_mer) {
	    mark = 1;
	    count = 0;
      }

      // "new" scoring system
      //printf("%d, %d, %s\n", len, bl->k_mer, cur);
      if (bloom_check (bl, cur, bl->k_mer)) {
          match_time++;
          if (prev == 1)
            conse++;
          else {
              conse += bl->k_mer;
              prev = 1;
          }

          if (mark == 1) {
              match_s += (bl->k_mer - 1);
              mark = 0;
          } else
              match_s++;
        } else {
              prev = 0;
        }

        count++;
        length--;
        *cur++;
    }

  result = (float) (match_time * bl->k_mer + conse) /
  	   (float) (len * bl->k_mer - 2 * bl->dx + len - bl->k_mer + 1);

#pragma omp atomic
  File_head->hits += match_time;
#pragma omp atomic
  File_head->all_k += (len - bl->k_mer);

  if (result >= tole_rate)
    return match_s;
  else
    return 0;
}


int
get_parainfo (char *full, Queue * head)
{
#ifdef DEBUG
  printf ("distributing...\n");
#endif
	  int type = 0;
      char *previous = NULL;
	  char *temp = full;
#ifndef __clang__
	  int cores = omp_get_num_procs ();
#else
	  int cores = 1;
#endif
	  short add = 0;
      int offset = 0;
	  Queue *pos = head;
      int length = 0;

      if (full != NULL) {
          offset = strlen(full) / cores;
          if (*full == '>')
            type = 1;
          else if (*full == '@')
            type = 2;
          else {
                fprintf(stderr, "File format not supported\n");
                exit(EXIT_FAILURE);
          }
      }
      
      if (type == 1) {
              for (add = 0; add < cores; add++) {
                  Queue *x = NEW (Queue);
                  if (add == 0 && *full != '>')
                    temp = strchr (full, '>');	//drop the possible fragment

                  if (add != 0)
                    temp = strchr (full + offset * add, '>');
                  x->location = temp;
                  x->number = &add;
                  x->next = pos->next;
                  pos->next = x;
                  pos = pos->next;
              }

	  } else {
              //char *tx = strchr(full,'\n');
              //length = strchr(tx+1,'\n')-(tx+1);
     
	      for (add = 0; add < cores; add++) {
              char *tx = strchr(full,'\n');
              length = strchr(tx+1,'\n')-(tx+1);
              
	      Queue *x = NEW (Queue);
              x->location = NULL;
              //char *tx = strchr(full,'\n');
              //length = strchr(tx+1,'\n')-(tx+1);
              if (add != 0)
                  temp = fastq_relocate(full, offset*add, length);
                       
              if (previous!=temp) {
                  previous = temp;
                  x->location = temp;
                  x->number = &add;
                  x->next = pos->next;
                  pos->next = x;
                  pos = pos->next;
              }
	      }
    }

  return type;
}

char *
fastq_relocate (char *data, int offset, int length)
{
  char *target = NULL;
  int current_length = 0, read_length = 0;
  if (data != NULL && offset != 0)
    {
      target = strstr (data + offset, "\n+");
      if (!target)
	return NULL;
      else
	{
	  current_length = strchr (target + 1, '\n') - target + 1;
	  read_length = fq_read_length (target - 1);
	  if (read_length != current_length)
	    target = strchr (target + 1, '\n') + 1;
	  if (target != NULL)
	    target = strchr (target + 1, '\n') + 1;
	}
    }
  return target;
}

int
dx_add (int k_mer)
{
  int x;
  int y = 0;
  for (x = 1; x < k_mer; x++)
    y += x;
  return y;
}

int
fq_read_length (char *data)
{
  char *origin = data;
  while (*data != '\n')
    data--;
  return origin - data;
}

void isodate(char* buf) {
    /* Borrowed from: https://raw.github.com/jordansissel/experiments/bd58235/c/time/iso8601.c */
    struct timeval tv;
    struct tm tm;
    char timestamp[] = "YYYY-MM-ddTHH:mm:ss.SSS+0000";

    /* Get the current time at high precision; could also use clock_gettime() for
     * even higher precision times if we want it. */
    gettimeofday(&tv, NULL);

    /* convert to time to 'struct tm' for use with strftime */
    localtime_r(&tv.tv_sec, &tm);

    /* format the time */
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S.000%z", &tm);

    /* but, since strftime() can't subsecond precision, we have to hack it
     * in manually. '20' is the string offset of the subsecond value in our
     * timestamp string. Also, because sprintf always writes a null, we have to
     * write the subsecond value as well as the rest of the string already there.
     */
    sprintf(timestamp + 20, "%03d%s", tv.tv_usec / 1000, timestamp + 23);
    sprintf(buf, "%s", timestamp);
}

char*
report(F_set * File_head, char* query, char* fmt, char* prefix)
{
  static char buffer[800] = { 0 };
  static char timestamp[40] = { 0 };
  float contamination_rate = (float) (File_head->reads_contam) /
                             (float) (File_head->reads_num);

  if(!fmt){
      return "err";
  // JSON output format
  } else if(!strcmp(fmt, "json")) {
      isodate(timestamp);
      sprintf(buffer,
"{\n"
"\t\"timestamp\": \"%s\"\n"
"\t\"sample\": \"%s\"\n"
"\t\"bloom_filter\": \"%s\"\n"
"\t\"total_read_count\": %lld,\n"
"\t\"contaminated_reads\": %lld,\n"
"\t\"total_hits\": %lld,\n"
"\t\"contamination_rate\": %f,\n"
"}\n",  timestamp, query, basename(File_head->filename),
        File_head->reads_num, File_head->reads_contam, File_head->hits,
        contamination_rate);

  // TSV output format
  } else if (!strcmp(fmt, "tsv")) {
      sprintf(buffer,
"sample\tbloom_filter\ttotal_read_count\tcontaminated_reads\tcontamination_rate\n"
"%s\t%s\t%lld\t%lld\t%f\n", query, basename(File_head->filename),
                            File_head->reads_num, File_head->reads_contam,
                            contamination_rate);
  }

  return buffer;
}

char* 
substr(const char* str, size_t begin, size_t len) 
{
	if (str == 0)
		return 0; 
	return strndup(str + begin, len); 
} 

BIGCAST
get_filesize (char* filename)
{
  struct stat buf;
  if (stat (filename, &buf) != -1){
    return buf.st_size;
  }else{
	fprintf (stderr, "%s: %s\n", filename, strerror(errno));
	exit(EXIT_FAILURE);
  }
}
