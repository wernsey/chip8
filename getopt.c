/*
 * public domain getopt(3) by Keith Bostic from mod.sources
 * https://groups.google.com/forum/#!topic/mod.sources/2FNKQgL81d0
 * Changed whitespace and return value, modernised prototype,
 * replaced index with strchr()
 */
#include <stdio.h>
#include <string.h>

/*
* get option letter from argument vector
*/
int opterr = 1, /* useless, never set or used */
    optind = 1, /* index into parent argv vector */
    optopt;     /* character checked for validity */
char *optarg;   /* argument associated with option */

#define BADCH        (int)'?'
#define EMSG        ""
#define tell(s)        fputs(*nargv,stderr);fputs(s,stderr); \
fputc(optopt,stderr);fputc('\n',stderr);return(BADCH);

int getopt(int nargc, char **nargv, char *ostr)
{
    static char        *place = EMSG;   /* option letter processing */
    register char        *oli;          /* option letter list index */

    if(!*place) {                       /* update scanning pointer */
        if(optind >= nargc || *(place = nargv[optind]) != '-' || !*++place)
            return(EOF);
        if (*place == '-') {            /* found "--" */
            ++optind;
            return(EOF);
        }
    }
    /* option letter okay? */
    if ((optopt = (int)*place++) == (int)':' || !(oli = strchr(ostr,optopt))) {
        if(!*place) ++optind;
        tell(": illegal option -- ");
    }
    if (*++oli != ':') {                /* don't need argument */
        optarg = NULL;
        if (!*place)
            ++optind;
    } else {                            /* need an argument */
        if (*place)
            optarg = place;             /* no white space */
        else if (nargc <= ++optind) {   /* no arg */
            place = EMSG;
            tell(": option requires an argument -- ");
        } else
            optarg = nargv[optind];     /* white space */
        place = EMSG;
        ++optind;
    }
    return(optopt);                     /* dump back option letter */
}