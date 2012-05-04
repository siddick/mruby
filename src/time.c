/*
** time.c - Time class
**
** See Copyright Notice in mruby.h
*/


#include "mruby.h"
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <math.h>
#include "error.h"
#include "mruby/class.h"
#include "mruby/string.h"

/* Since we are limited to using ISO C89, this implementation is based 
* on time_t. That means the resolution of time is only precise to the 
* second level. Also, there are only 2 timezones, namely UTC and LOCAL.
*/

#ifndef mrb_bool_value
#define mrb_bool_value(val) ((val) ? mrb_true_value() : mrb_false_value())
#endif


enum mrb_timezone {
  MRB_TIMEZONE_NONE   = 0,
  MRB_TIMEZONE_UTC    = 1, 
  MRB_TIMEZONE_LOCAL  = 2,
  MRB_TIMEZONE_LAST   = 3
};

static char * mrb_timezone_names[] = {
  "none",
  "UTC",
  "LOCAL",
   NULL
};

struct mrb_time {
  time_t              sec;
  time_t              usec;
  enum mrb_timezone   timezone;
  struct tm           datetime;
};

static void
mrb_time_free(mrb_state *mrb, void * ptr) {
  mrb_free(mrb, ptr);
}

static struct mrb_data_type mrb_time_type = { "Time", mrb_time_free };

/** Updates the datetime of a mrb_time based on it's timezone and
seconds setting. Returns self on cussess, NULL of failure. */
struct mrb_time * mrb_time_update_datetime(struct mrb_time * self) {
  struct tm * aid;
  if(self->timezone == MRB_TIMEZONE_UTC) { 
    aid = gmtime(&self->sec);
  }
  else {
    aid = localtime(&self->sec);
  }
  if(!aid) return NULL;
  self->datetime = *aid; // copy data
  return self;  
}

static mrb_value
mrb_time_wrap(mrb_state *mrb, struct RClass *tc, struct mrb_time *tm) {
  return mrb_obj_value(Data_Wrap_Struct(mrb, tc, &mrb_time_type, tm));
}


/* Allocates a mrb_time object and initializes it. */
static struct mrb_time * 
mrb_time_make(mrb_state * mrb, mrb_float seconds, int timezone) {
  struct mrb_time * res;

  res = mrb_malloc(mrb, sizeof(struct mrb_time));
  /* this trick below may or may not work on all systems? */
  res->sec      = (time_t)seconds;
  res->usec     = (seconds - res->sec) * 1.0e6;
  res->timezone = MRB_TIMEZONE_LOCAL;
  mrb_time_update_datetime(res);
  return res;
}

/* Allocates a new Time object with given millis value. */
static mrb_value
mrb_time_new(mrb_state *mrb, mrb_value self) {
  mrb_float f;
  struct mrb_time * tm;  

  mrb_get_args(mrb, "f", &f);
  tm = mrb_malloc(mrb, sizeof(*tm));
  tm->sec      = (time_t) (f / 1.0e6);
  tm->usec     = 0;
  tm->timezone = MRB_TIMEZONE_LOCAL;
  mrb_time_update_datetime(tm);
  return mrb_time_wrap(mrb, mrb_class_ptr(self), tm);
}


/* Allocates a new Time object with given millis value. */
static mrb_value
mrb_time_now(mrb_state *mrb, mrb_value self) {
  struct mrb_time * tm;  

  tm = mrb_malloc(mrb, sizeof(*tm));
  tm->sec  = time(NULL);
  tm->usec     = 0;
  tm->timezone = MRB_TIMEZONE_LOCAL;
  mrb_time_update_datetime(tm);
  return mrb_time_wrap(mrb, mrb_class_ptr(self), tm);
}

/* 15.2.19.6.1 */
/* Creates an instance of time at the given time in seconds, etc. */
static mrb_value
mrb_time_at(mrb_state *mrb, mrb_value self) { 
  mrb_value s_arg, m_arg;
  struct mrb_time * res, *other;
  mrb_get_args(mrb, "oo", &s_arg, &m_arg);
  if(!RTEST(s_arg)) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "Need at least one argument.");
  }
  other = mrb_check_datatype(mrb, s_arg, &mrb_time_type);
  /** Check if a Time was passed... */
  if(other) {
    res = mrb_time_make(mrb, (mrb_float)other->sec, other->timezone);

  }
  else { 
    res = mrb_time_make(mrb, mrb_float(s_arg), MRB_TIMEZONE_LOCAL);
  }
  return mrb_time_wrap(mrb, mrb_class_ptr(self), res);
}

/* 15.2.19.6.1 */
/* Creates an instance of time at the given time in gmt time zone. */
static mrb_value
mrb_time_gm(mrb_state *mrb, mrb_value self) { 
  time_t nowsecs;
  mrb_float ayear = 0.0, amonth = 1.0, aday = 1.0, ahour = 0.0, 
  amin = 0.0, asec = 0.0, ausec = 0.0;
  struct tm nowtime;
  struct mrb_time * res;
  mrb_get_args(mrb, "fffffff",
                &ayear, &amonth, &aday, &ahour, &amin, &asec, &ausec);
  nowtime.tm_year  = (int) floor(ayear)  - 1900;
  nowtime.tm_mon   = (int) floor(amonth) - 1;
  nowtime.tm_mday  = (int) floor(aday);
  nowtime.tm_hour  = (int) floor(ahour);
  nowtime.tm_min   = (int) floor(amin);
  nowtime.tm_sec   = (int) floor(asec);
  nowtime.tm_isdst = -1;
  nowsecs = mktime(&nowtime);
  if (nowsecs < 0) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "Not a valid time.");
  }
         
  
  res = mrb_time_make(mrb, nowsecs, MRB_TIMEZONE_UTC);
  return mrb_time_wrap(mrb, mrb_class_ptr(self), res);
}


static mrb_value
mrb_time_eq(mrb_state *mrb, mrb_value self)
{
  mrb_value other;
  struct mrb_time *tm1, *tm2;

  mrb_get_args(mrb, "o", &other);
  tm1 = mrb_check_datatype(mrb, self, &mrb_time_type);
  tm2 = mrb_check_datatype(mrb, other, &mrb_time_type);
  if (!tm1 || !tm2) return mrb_false_value();
  if (tm1->sec == tm2->sec && tm1->usec == tm2->usec) {
    return mrb_true_value();
  }
  return mrb_false_value();
}

static mrb_value
mrb_time_cmp(mrb_state *mrb, mrb_value self)
{
  mrb_value other;
  struct mrb_time *tm1, *tm2;

  mrb_get_args(mrb, "o", &other);
  tm1 = mrb_check_datatype(mrb, self, &mrb_time_type);
  tm2 = mrb_check_datatype(mrb, other, &mrb_time_type);
  if (!tm1 || !tm2) return mrb_nil_value();
  if (tm1->sec > tm2->sec) {
    return mrb_fixnum_value(1);
  }
  else if (tm1->sec < tm2->sec) {
    return mrb_fixnum_value(-1);
  }
  /* tm1->sec == tm2->sec */
  if (tm1->usec > tm2->usec) {
    return mrb_fixnum_value(1);
  }
  else if (tm1->usec < tm2->usec) {
    return mrb_fixnum_value(-1);
  }
  return mrb_fixnum_value(0);
}

static mrb_value
mrb_time_plus(mrb_state *mrb, mrb_value self)
{
  mrb_float f;
  struct mrb_time *tm;

  mrb_get_args(mrb, "f", &f);
  tm = mrb_check_datatype(mrb, self, &mrb_time_type);
  f += tm->sec;
  f += (mrb_float)tm->usec / 1.0e6;
  return mrb_time_wrap(mrb, mrb_obj_class(mrb, self), mrb_time_make(mrb, f, tm->timezone));
}

static mrb_value
mrb_time_minus(mrb_state *mrb, mrb_value self)
{
  mrb_float f;
  struct mrb_time *tm;

  mrb_get_args(mrb, "f", &f);
  
  tm = mrb_check_datatype(mrb, self, &mrb_time_type);
  f -= tm->sec;
  f -= (mrb_float)tm->usec / 1.0e6;
  return mrb_time_wrap(mrb, mrb_obj_class(mrb, self), mrb_time_make(mrb, f, tm->timezone));
}

/* 15.2.19.7.30 */
/* Returns week day number of time. */
static mrb_value
mrb_time_wday(mrb_state *mrb, mrb_value self)
{
  struct mrb_time * tm;
  tm = mrb_check_datatype(mrb, self, &mrb_time_type);
  if(!tm) return mrb_nil_value();
  return mrb_fixnum_value((tm->datetime.tm_wday));
}

/* 15.2.19.7.31 */
/* Returns year day number of time. */
static mrb_value
mrb_time_yday(mrb_state *mrb, mrb_value self)
{
  struct mrb_time * tm;
  tm = mrb_check_datatype(mrb, self, &mrb_time_type);
  if(!tm) return mrb_nil_value();
  return mrb_fixnum_value((tm->datetime.tm_yday));
}

/* 15.2.19.7.32 */
/* Returns year of time. */
static mrb_value
mrb_time_year(mrb_state *mrb, mrb_value self)
{
  struct mrb_time * tm;
  tm = mrb_check_datatype(mrb, self, &mrb_time_type);
  if(!tm) return mrb_nil_value();
  return mrb_fixnum_value((tm->datetime.tm_year) + 1900);
}

/* 15.2.19.7.33 */
/* Returns name of time's timezone. */
static mrb_value
mrb_time_zone(mrb_state *mrb, mrb_value self)
{
  struct mrb_time * tm;
  tm = mrb_check_datatype(mrb, self, &mrb_time_type);
  if(!tm) return mrb_nil_value();
  if(tm->timezone <= MRB_TIMEZONE_NONE) return mrb_nil_value();
  if(tm->timezone >= MRB_TIMEZONE_LAST) return mrb_nil_value();
  return mrb_str_new2(mrb, mrb_timezone_names[tm->timezone]);
}

static const char *mon_names[] = {
  "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
};

static const char *wday_names[] = {
  "Sun", "Mon", "Tus", "Wed", "Thu", "Fri", "Sat",
};

/* 15.2.19.7.4 */
/* Returns a string that describes the time. */
static mrb_value
mrb_time_asctime(mrb_state *mrb, mrb_value self)
{
  struct mrb_time *tm;
  struct tm *d;
  char buf[256];

  tm = mrb_check_datatype(mrb, self, &mrb_time_type);
  if(!tm) return mrb_nil_value();
  d = &tm->datetime;
  snprintf(buf, 256, "%s %s %02d %02d:%02d:%02d %s%d",
	   wday_names[d->tm_wday], mon_names[d->tm_mon], d->tm_mday, 
	   d->tm_hour, d->tm_min, d->tm_sec,
	   tm->timezone == MRB_TIMEZONE_UTC ? "UTC " : "",
	   d->tm_year + 1900);
  return mrb_str_new2(mrb, buf);
}

/* 15.2.19.7.6 */
/* Returns the day in the month of the time. */
static mrb_value
mrb_time_day(mrb_state *mrb, mrb_value self)
{
  struct mrb_time * tm;
  tm = mrb_check_datatype(mrb, self, &mrb_time_type);
  if(!tm) return mrb_nil_value();
  return mrb_bool_value((tm->datetime.tm_isdst));
}


/* 15.2.19.7.7 */
/* Returns true if daylight saving was applied for this time. */
static mrb_value
mrb_time_dstp(mrb_state *mrb, mrb_value self)
{
  struct mrb_time * tm;
  tm = mrb_check_datatype(mrb, self, &mrb_time_type);
  if(!tm) return mrb_nil_value();
  return mrb_bool_value((tm->datetime.tm_isdst));
}

/* 15.2.19.7.15 */
/* Returns hour of time. */
static mrb_value
mrb_time_hour(mrb_state *mrb, mrb_value self)
{
  struct mrb_time * tm;
  tm = mrb_check_datatype(mrb, self, &mrb_time_type);
  if(!tm) return mrb_nil_value();
  return mrb_fixnum_value((tm->datetime.tm_hour));
}

/* 15.2.19.7.16 */
/* Initializes a time by setting the amount of milliseconds since the epoch.*/
static mrb_value
mrb_time_initialize(mrb_state *mrb, mrb_value self) {
  mrb_float micros  = 0.0;
  struct mrb_time * tm;
  tm = mrb_check_datatype(mrb, self, &mrb_time_type);
  if(!tm) return mrb_nil_value();
  mrb_get_args(mrb, "f", &micros);
  tm->sec  = (time_t) floor(micros / 1.0e6);
  tm->timezone = MRB_TIMEZONE_LOCAL;
  mrb_time_update_datetime(tm);
  return self;
}

/* 15.2.19.7.17(x) */
/* Initializes a copy of this time object. */
static mrb_value
mrb_time_initialize_copy(mrb_state *mrb, mrb_value copy)
{
  mrb_value src;
  mrb_get_args(mrb, "o", &src);

  if (mrb_obj_equal(mrb, copy, src)) return copy;
  if (!mrb_obj_is_instance_of(mrb, src, mrb_obj_class(mrb, copy))) {
    mrb_raise(mrb, E_TYPE_ERROR, "wrong argument class");
  }
  memcpy(DATA_PTR(copy), DATA_PTR(src), sizeof(struct mrb_time));
  return copy;
}

/* 15.2.19.7.18 */
/* Sets the timezone attribute of the Time object to LOCAL. */
static mrb_value
mrb_time_localtime(mrb_state *mrb, mrb_value self)
{
  struct mrb_time * tm;
  tm = mrb_check_datatype(mrb, self, &mrb_time_type);
  if(!tm) return self;
  tm->timezone = MRB_TIMEZONE_LOCAL;
  return self;
}

/* 15.2.19.7.19 */
/* Returns day of month of time. */
static mrb_value
mrb_time_mday(mrb_state *mrb, mrb_value self)
{
  struct mrb_time * tm;
  tm = mrb_check_datatype(mrb, self, &mrb_time_type);
  if(!tm) return mrb_nil_value();
  return mrb_fixnum_value((tm->datetime.tm_mday));
}

/* 15.2.19.7.20 */
/* Returns minutes of time. */
static mrb_value
mrb_time_min(mrb_state *mrb, mrb_value self)
{
  struct mrb_time * tm;
  tm = mrb_check_datatype(mrb, self, &mrb_time_type);
  if(!tm) return mrb_nil_value();
  return mrb_fixnum_value((tm->datetime.tm_min));
}

/* 15.2.19.7.21 and 15.2.19.7.22 */
/* Returns month of time. */
static mrb_value
mrb_time_mon(mrb_state *mrb, mrb_value self)
{
  struct mrb_time * tm;
  tm = mrb_check_datatype(mrb, self, &mrb_time_type);
  if(!tm) return mrb_nil_value();
  return mrb_fixnum_value((tm->datetime.tm_mon) + 1);
}

/* 15.2.19.7.23 */
/* Returns seconds in minute of time. */
static mrb_value
mrb_time_sec(mrb_state *mrb, mrb_value self)
{
  struct mrb_time * tm;
  tm = mrb_check_datatype(mrb, self, &mrb_time_type);
  if(!tm) return mrb_nil_value();
  return mrb_fixnum_value((tm->datetime.tm_sec));
}


/* 15.2.19.7.24 */
/* Returns a Float with the time since the epoch in seconds. */
static mrb_value
mrb_time_to_f(mrb_state *mrb, mrb_value self)
{
  struct mrb_time * tm;
  tm = mrb_check_datatype(mrb, self, &mrb_time_type);
  if(!tm) return mrb_nil_value();
  return mrb_float_value(((mrb_float)tm->sec + (mrb_float)tm->usec/1.0e6));
}

/* 15.2.19.7.25 */
/* Returns a Fixnum with the time since the epoch in seconds. */
static mrb_value
mrb_time_to_i(mrb_state *mrb, mrb_value self)
{
  struct mrb_time * tm;
  tm = mrb_check_datatype(mrb, self, &mrb_time_type);
  if(!tm) return mrb_nil_value();
  return mrb_fixnum_value((tm->sec));
}

/* 15.2.19.7.26 */
/* Returns a Float with the time since the epoch in microseconds. */
static mrb_value
mrb_time_usec(mrb_state *mrb, mrb_value self)
{
  struct mrb_time * tm;
  tm = mrb_check_datatype(mrb, self, &mrb_time_type);
  if(!tm) return mrb_nil_value();
  return mrb_fixnum_value(tm->usec);
}

/* 15.2.19.7.27 */
/* Sets the timzeone attribute of the Time object to UTC. */
static mrb_value
mrb_time_utc(mrb_state *mrb, mrb_value self)
{
  struct mrb_time * tm;
  tm = mrb_check_datatype(mrb, self, &mrb_time_type);
  if(!tm) return self;
  tm->timezone = MRB_TIMEZONE_UTC;
  return self;
}

/* 15.2.19.7.28 */
/* Returns true if this time is in the UTC timze zone false if not. */
static mrb_value
mrb_time_utcp(mrb_state *mrb, mrb_value self)
{
  struct mrb_time * tm;
  tm = mrb_check_datatype(mrb, self, &mrb_time_type);
  if(!tm) return mrb_nil_value();
  return mrb_bool_value((tm->timezone == MRB_TIMEZONE_UTC));
}

           

void
mrb_init_time(mrb_state *mrb) {
  struct RClass * tc;
  /* ISO 15.2.19.2 */
  tc = mrb_define_class(mrb, "Time", mrb->object_class);
  mrb_include_module(mrb, tc, mrb_class_get(mrb, "Comparable"));
  mrb_define_class_method(mrb, tc, "new", mrb_time_new, ARGS_ANY());
  mrb_define_class_method(mrb, tc, "now", mrb_time_now, ARGS_NONE());
  mrb_define_class_method(mrb, tc, "at", mrb_time_at, ARGS_ANY());
  mrb_define_class_method(mrb, tc, "gm", mrb_time_gm, ARGS_REQ(1)|ARGS_OPT(6));

  mrb_define_method(mrb, tc, "=="     , mrb_time_eq     , ARGS_REQ(1));
  mrb_define_method(mrb, tc, "<=>"    , mrb_time_cmp    , ARGS_REQ(1));
  mrb_define_method(mrb, tc, "+"      , mrb_time_plus   , ARGS_REQ(1));
  mrb_define_method(mrb, tc, "-"      , mrb_time_minus  , ARGS_REQ(1));
  mrb_define_method(mrb, tc, "to_s"   , mrb_time_asctime, ARGS_NONE());
  mrb_define_method(mrb, tc, "asctime", mrb_time_asctime, ARGS_NONE());
  mrb_define_method(mrb, tc, "ctime"  , mrb_time_asctime, ARGS_NONE());
  mrb_define_method(mrb, tc, "day"    , mrb_time_day    , ARGS_NONE());
  mrb_define_method(mrb, tc, "dst?"   , mrb_time_dstp   , ARGS_NONE());
  mrb_define_method(mrb, tc, "gmt?"   , mrb_time_utcp   , ARGS_NONE());
  mrb_define_method(mrb, tc, "gmtime" , mrb_time_utc    , ARGS_NONE());
  mrb_define_method(mrb, tc, "hour"   , mrb_time_hour, ARGS_NONE());
  mrb_define_method(mrb, tc, "localtime", mrb_time_localtime, ARGS_NONE());
  mrb_define_method(mrb, tc, "mday"   , mrb_time_mday, ARGS_NONE());
  mrb_define_method(mrb, tc, "min"    , mrb_time_min, ARGS_NONE());
   
  mrb_define_method(mrb, tc, "mon"  , mrb_time_mon, ARGS_NONE());
  mrb_define_method(mrb, tc, "month", mrb_time_mon, ARGS_NONE());
  
  mrb_define_method(mrb, tc, "sec" , mrb_time_sec, ARGS_NONE());
  mrb_define_method(mrb, tc, "to_i", mrb_time_to_i, ARGS_NONE());
  mrb_define_method(mrb, tc, "to_f", mrb_time_to_f, ARGS_NONE());
  mrb_define_method(mrb, tc, "usec", mrb_time_usec, ARGS_NONE());
  mrb_define_method(mrb, tc, "utc" , mrb_time_utc, ARGS_NONE());
  mrb_define_method(mrb, tc, "utc?", mrb_time_utcp, ARGS_NONE());
  mrb_define_method(mrb, tc, "wday", mrb_time_wday, ARGS_NONE());
  mrb_define_method(mrb, tc, "yday", mrb_time_yday, ARGS_NONE());
  mrb_define_method(mrb, tc, "year", mrb_time_year, ARGS_NONE());
  mrb_define_method(mrb, tc, "zone", mrb_time_zone, ARGS_NONE());
  
  mrb_define_method(mrb, tc, "initialize", mrb_time_initialize, ARGS_REQ(1));
  mrb_define_method(mrb, tc, "initialize_copy", mrb_time_initialize_copy, ARGS_REQ(1));
}