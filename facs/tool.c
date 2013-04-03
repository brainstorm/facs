#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <dirent.h>
#include <limits.h>
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
  char *p = begin;
  int distance = length;
  int signal = 0, result = 0;
  char *previous, *key = (char *) malloc (bl->k_mer * sizeof (char) + 1);

  while (distance > bl->k_mer)
    {
      if (signal == 1)
	break;

      if (distance >= bl->k_mer) {
	  memcpy (key, p, sizeof (char) * bl->k_mer);	//need to be tested
	  key[bl->k_mer] = '\0';
	  p += bl->k_mer;
	  previous = p;
	  distance -= bl->k_mer;
      } else {
	  memcpy (key, previous + distance, sizeof (char) * bl->k_mer);
	  p += (bl->k_mer - distance);
	  signal = 1;
      }

      if (model == 'r')
	rev_trans (key);

      if (bloom_check (bl, key)) {
	  result =
	    fastq_full_check (bl, begin, length, model, tole_rate, File_head);
	  if (result > 0)
	    return result;
	  else if (model == 'n')
	    break;
      }
    }				//outside while

  if (model == 'r')
    return 0;
  else
    return query_read(begin, length, 'r', bl, tole_rate, File_head);
}

/*-------------------------------------*/
int
fastq_full_check (bloom * bl, char *p, int distance, char model, float tole_rate, F_set * File_head)
{
  int length = distance;
  int count = 0, match_s = 0, mark = 1, match_time = 0;
  float result;
  char *key = (char *) malloc (bl->k_mer * sizeof (char) + 1);
  short prev = 0, conse = 0;

  while (distance >= bl->k_mer) {
      memcpy (key, p, sizeof (char) * bl->k_mer);
      key[bl->k_mer] = '\0';
      p += 1;

      if (model == 'r')
	rev_trans (key);

      if (count >= bl->k_mer) {
	  mark = 1;
	  count = 0;
      }

      if (strlen (key) == bl->k_mer) {
	  if (bloom_check (bl, key)) {
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
	    }

	  else {
	      prev = 0;
	  }
	  
	  count++;
	}			//outside if
      distance--;
    }				// end while

  free (key);
  result = (float) (match_time * bl->k_mer + conse) /
  	   (float) (length * bl->k_mer - 2 * bl->dx + length - bl->k_mer + 1);

#pragma omp atomic
  File_head->hits += match_time;
#pragma omp atomic
  File_head->all_k += (length - bl->k_mer);

  if (result >= tole_rate)
    return match_s;
  else
    return 0;
}

/*-------------------------------------*/
int
fasta_read_check (char *begin, char *next, char model, bloom * bl, float tole_rate, F_set * File_head)
{

  char *p = strchr (begin + 1, '\n') + 1;

  if (!p || *p == '>')
    return 1;

  int n, m, result, count_enter;
  char *key = (char *) malloc ((bl->k_mer + 1) * sizeof (char));
  char *pre_key = (char *) malloc ((bl->k_mer + 1) * sizeof (char));

  key[bl->k_mer] = '\0';

  while (p != next)
    {
      while (n < bl->k_mer)
	{
	  if (p[m] == '>' || p[m] == '\0')
	    {
	      m--;
	      break;
	    }

	  if (p[m] != '\r' && p[m] != '\n')
	    key[n++] = p[m];
	  else
	    count_enter++;
	  m++;
	}			//inner while

      if (m == 0)
	break;

      if (strlen (key) == bl->k_mer)
	memcpy (pre_key, key, sizeof (char) * (bl->k_mer + 1));

      else
	{
	  char *temp_key = (char *) malloc (bl->k_mer * sizeof (char));

	  memcpy (temp_key, pre_key + strlen (key), bl->k_mer - strlen (key));

	  memcpy (temp_key + bl->k_mer - strlen (key), key, sizeof (char) * (strlen (key) + 1));

	  free (key);

	  key = temp_key;

	}
      p += m;

      n = 0;

      m = 0;

      if (model == 'r')
	rev_trans (key);

      if (bloom_check (bl, key))
	{
	  result = fasta_full_check (bl, begin, next, model, tole_rate, File_head);
	  if (result > 0)
	    return result;
	  //else if (model == 'n')     //use recursion to check the sequence forward and backward
	  //    return fasta_read_check (begin, next, 'r', bl);
	  else if (model == 'n')
	    break;
	}

      //memset (key, 0, bl->k_mer);
    }				//outside while
  if (model == 'r')
    return 0;
  else
    return fasta_read_check (begin, next, 'r', bl, tole_rate, File_head);
}

/*-------------------------------------*/
int
fasta_full_check (bloom * bl, char *begin, char *next, char model, float tole_rate, F_set * File_head)
{
  int match_s = 0, count = 0, mark = 1;
  int n = 0, m = 0, count_enter = 0, match_time = 0;
  short previous = 0, conse = 0;
  float result;

  char *key = (char *) malloc ((bl->k_mer + 1) * sizeof (char));
  begin = strchr (begin + 1, '\n') + 1;
  char *p = begin;

  while (p != next) {
      if (*p == '\n')
	count_enter++;
      p++;
  }

  p = begin;

  while (*p != '>' && *p != '\0') {
      while (n < bl->k_mer) {
	  if (p[m] == '>' || p[m] == '\0') {
	      m--;
	      break;
	    }

	  if (p[m] != '\r' && p[m] != '\n')
	    key[n++] = p[m];

	  m++;
	}
      key[n] = '\0';

      if (model == 'r')
	rev_trans (key);
      //printf("key->%s\n",key);
      if (count >= bl->k_mer)
	{
	  mark = 1;
	  count = 0;
	}
      if (strlen (key) == bl->k_mer)
	{
	  if (bloom_check (bl, key))
	    {
	      match_time++;
	      if (previous == 1)
		conse++;
	      else
		{
		  conse += bl->k_mer;
		  previous = 1;
		}
	      if (mark == 1)
		{
		  match_s += (bl->k_mer - 1);
		  mark = 0;
		}
	      else
		match_s++;
	    }

	  else
	    {
	      previous = 0;
	      //printf("unhit--->\n");
	    }

	  count++;
	}			//outside if
      //printf("score->%d\n",match_s);
      p++;
      if (p[0] == '\n')
	p++;
      n = 0;
      m = 0;
    }				// end of while
  result = (float) (match_time * bl->k_mer + conse) / (float) ((next - begin - count_enter) * bl->k_mer - 2 * bl->dx + (next - begin - count_enter) - bl->k_mer + 1);

#pragma omp atomic
  File_head->hits += match_time;
#pragma omp atomic
  File_head->all_k += (next - begin - count_enter - bl->k_mer);

  if (result >= tole_rate)	//match >tole_rate considered as contaminated
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
       //   Queue *x = NEW (Queue);
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

void
report(F_set * File_head, char* query, char* fmt, char* prefix)
{
  char buffer[200] = { 0 };
  float contamination_rate = (float) (File_head->reads_contam) /
                             (float) (File_head->reads_num);

  if(!fmt){
      return;
  // JSON output format
  } else if(!strcmp(fmt, "json")) {
      isodate(buffer);

      printf("{\n");
      printf("\t\"timestamp\": \"%s\"\n", buffer);
      printf("\t\"sample\": \"%s\"\n", basename(query)); //sample (query)
      printf("\t\"bloom_filter\": \"%s\"\n", basename(File_head->filename)); //reference
      printf("\t\"total_read_count\": %lld,\n", File_head->reads_num);
      printf("\t\"contaminated_reads\": %lld,\n", File_head->reads_contam);
      printf("\t\"total_hits\": %lld,\n", File_head->hits);
      printf("\t\"contamination_rate\": %f,\n", contamination_rate);
      printf("}\n");

  // TSV output format
  } else if (!strcmp(fmt, "tsv")) {
      printf("sample\tbloom_filter\ttotal_read_count\t\
contaminated_reads\tcontamination_rate\n");

      printf("%s\t%s\t%lld\t%lld\t%f\n", basename(query),
              basename(File_head->filename), File_head->reads_num,
              File_head->reads_contam, contamination_rate);
  }
}
