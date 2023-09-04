/*
** 2001 September 15
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
*************************************************************************
** Main file for the SQLite library.  The routines in this file
** implement the programmer interface to the library.  Routines in
** other files are for internal use by SQLite and should not be
** accessed by users of the library.
*/
#include "sqliteInt.h"

#ifdef SQLITE_ENABLE_FTS3
# include "fts3.h"
#endif
#ifdef SQLITE_ENABLE_RTREE
# include "rtree.h"
#endif
#if defined(SQLITE_ENABLE_ICU) || defined(SQLITE_ENABLE_ICU_COLLATIONS)
# include "sqliteicu.h"
#endif



#ifndef SQLITE_AMALGAMATION
# if !defined(NDEBUG) && !defined(SQLITE_DEBUG)
#  define NDEBUG 1
# endif
# if defined(NDEBUG) && defined(SQLITE_DEBUG)
#  undef NDEBUG
# endif
# include <string.h>
# include <stdio.h>
# include <stdlib.h>
# include <assert.h>
# define ALWAYS(X)  1
# define NEVER(X)   0
  typedef unsigned char u8;
  typedef unsigned short u16;
#endif
#include <ctype.h>

#ifndef SQLITE_OMIT_VIRTUALTABLE

/*
** Character classes for ASCII characters:
**
**   0   ''        Silent letters:   H W
**   1   'A'       Any vowel:   A E I O U (Y)
**   2   'B'       A bilabeal stop or fricative:  B F P V W
**   3   'C'       Other fricatives or back stops:  C G J K Q S X Z
**   4   'D'       Alveolar stops:  D T
**   5   'H'       Letter H at the beginning of a word
**   6   'L'       Glide:  L
**   7   'R'       Semivowel:  R
**   8   'M'       Nasals:  M N
**   9   'Y'       Letter Y at the beginning of a word.
**   10  '9'       Digits: 0 1 2 3 4 5 6 7 8 9
**   11  ' '       White space
**   12  '?'       Other.
*/
#define CCLASS_SILENT         0
#define CCLASS_VOWEL          1
#define CCLASS_B              2
#define CCLASS_C              3
#define CCLASS_D              4
#define CCLASS_H              5
#define CCLASS_L              6
#define CCLASS_R              7
#define CCLASS_M              8
#define CCLASS_Y              9
#define CCLASS_DIGIT         10
#define CCLASS_SPACE         11
#define CCLASS_OTHER         12

/*
** The following table gives the character class for non-initial ASCII
** characters.
*/
static const unsigned char midClass[] = {
 /*   */ CCLASS_OTHER,    /*   */ CCLASS_OTHER,   /*   */ CCLASS_OTHER,
 /*   */ CCLASS_OTHER,    /*   */ CCLASS_OTHER,   /*   */ CCLASS_OTHER,
 /*   */ CCLASS_OTHER,    /*   */ CCLASS_OTHER,   /*   */ CCLASS_OTHER,
 /*   */ CCLASS_SPACE,    /*   */ CCLASS_OTHER,   /*   */ CCLASS_OTHER,
 /*   */ CCLASS_SPACE,    /*   */ CCLASS_SPACE,   /*   */ CCLASS_OTHER,
 /*   */ CCLASS_OTHER,    /*   */ CCLASS_OTHER,   /*   */ CCLASS_OTHER,
 /*   */ CCLASS_OTHER,    /*   */ CCLASS_OTHER,   /*   */ CCLASS_OTHER,
 /*   */ CCLASS_OTHER,    /*   */ CCLASS_OTHER,   /*   */ CCLASS_OTHER,
 /*   */ CCLASS_OTHER,    /*   */ CCLASS_OTHER,   /*   */ CCLASS_OTHER,
 /*   */ CCLASS_OTHER,    /*   */ CCLASS_OTHER,   /*   */ CCLASS_OTHER,
 /*   */ CCLASS_OTHER,    /*   */ CCLASS_OTHER,   /*   */ CCLASS_SPACE,
 /* ! */ CCLASS_OTHER,    /* " */ CCLASS_OTHER,   /* # */ CCLASS_OTHER,
 /* $ */ CCLASS_OTHER,    /* % */ CCLASS_OTHER,   /* & */ CCLASS_OTHER,
 /* ' */ CCLASS_SILENT,   /* ( */ CCLASS_OTHER,   /* ) */ CCLASS_OTHER,
 /* * */ CCLASS_OTHER,    /* + */ CCLASS_OTHER,   /* , */ CCLASS_OTHER,
 /* - */ CCLASS_OTHER,    /* . */ CCLASS_OTHER,   /* / */ CCLASS_OTHER,
 /* 0 */ CCLASS_DIGIT,    /* 1 */ CCLASS_DIGIT,   /* 2 */ CCLASS_DIGIT,
 /* 3 */ CCLASS_DIGIT,    /* 4 */ CCLASS_DIGIT,   /* 5 */ CCLASS_DIGIT,
 /* 6 */ CCLASS_DIGIT,    /* 7 */ CCLASS_DIGIT,   /* 8 */ CCLASS_DIGIT,
 /* 9 */ CCLASS_DIGIT,    /* : */ CCLASS_OTHER,   /* ; */ CCLASS_OTHER,
 /* < */ CCLASS_OTHER,    /* = */ CCLASS_OTHER,   /* > */ CCLASS_OTHER,
 /* ? */ CCLASS_OTHER,    /* @ */ CCLASS_OTHER,   /* A */ CCLASS_VOWEL,
 /* B */ CCLASS_B,        /* C */ CCLASS_C,       /* D */ CCLASS_D,
 /* E */ CCLASS_VOWEL,    /* F */ CCLASS_B,       /* G */ CCLASS_C,
 /* H */ CCLASS_SILENT,   /* I */ CCLASS_VOWEL,   /* J */ CCLASS_C,
 /* K */ CCLASS_C,        /* L */ CCLASS_L,       /* M */ CCLASS_M,
 /* N */ CCLASS_M,        /* O */ CCLASS_VOWEL,   /* P */ CCLASS_B,
 /* Q */ CCLASS_C,        /* R */ CCLASS_R,       /* S */ CCLASS_C,
 /* T */ CCLASS_D,        /* U */ CCLASS_VOWEL,   /* V */ CCLASS_B,
 /* W */ CCLASS_B,        /* X */ CCLASS_C,       /* Y */ CCLASS_VOWEL,
 /* Z */ CCLASS_C,        /* [ */ CCLASS_OTHER,   /* \ */ CCLASS_OTHER,
 /* ] */ CCLASS_OTHER,    /* ^ */ CCLASS_OTHER,   /* _ */ CCLASS_OTHER,
 /* ` */ CCLASS_OTHER,    /* a */ CCLASS_VOWEL,   /* b */ CCLASS_B,
 /* c */ CCLASS_C,        /* d */ CCLASS_D,       /* e */ CCLASS_VOWEL,
 /* f */ CCLASS_B,        /* g */ CCLASS_C,       /* h */ CCLASS_SILENT,
 /* i */ CCLASS_VOWEL,    /* j */ CCLASS_C,       /* k */ CCLASS_C,
 /* l */ CCLASS_L,        /* m */ CCLASS_M,       /* n */ CCLASS_M,
 /* o */ CCLASS_VOWEL,    /* p */ CCLASS_B,       /* q */ CCLASS_C,
 /* r */ CCLASS_R,        /* s */ CCLASS_C,       /* t */ CCLASS_D,
 /* u */ CCLASS_VOWEL,    /* v */ CCLASS_B,       /* w */ CCLASS_B,
 /* x */ CCLASS_C,        /* y */ CCLASS_VOWEL,   /* z */ CCLASS_C,
 /* { */ CCLASS_OTHER,    /* | */ CCLASS_OTHER,   /* } */ CCLASS_OTHER,
 /* ~ */ CCLASS_OTHER,    /*   */ CCLASS_OTHER,   
};
/* 
** This tables gives the character class for ASCII characters that form the
** initial character of a word.  The only difference from midClass is with
** the letters H, W, and Y.
*/
static const unsigned char initClass[] = {
 /*   */ CCLASS_OTHER,    /*   */ CCLASS_OTHER,   /*   */ CCLASS_OTHER,
 /*   */ CCLASS_OTHER,    /*   */ CCLASS_OTHER,   /*   */ CCLASS_OTHER,
 /*   */ CCLASS_OTHER,    /*   */ CCLASS_OTHER,   /*   */ CCLASS_OTHER,
 /*   */ CCLASS_SPACE,    /*   */ CCLASS_OTHER,   /*   */ CCLASS_OTHER,
 /*   */ CCLASS_SPACE,    /*   */ CCLASS_SPACE,   /*   */ CCLASS_OTHER,
 /*   */ CCLASS_OTHER,    /*   */ CCLASS_OTHER,   /*   */ CCLASS_OTHER,
 /*   */ CCLASS_OTHER,    /*   */ CCLASS_OTHER,   /*   */ CCLASS_OTHER,
 /*   */ CCLASS_OTHER,    /*   */ CCLASS_OTHER,   /*   */ CCLASS_OTHER,
 /*   */ CCLASS_OTHER,    /*   */ CCLASS_OTHER,   /*   */ CCLASS_OTHER,
 /*   */ CCLASS_OTHER,    /*   */ CCLASS_OTHER,   /*   */ CCLASS_OTHER,
 /*   */ CCLASS_OTHER,    /*   */ CCLASS_OTHER,   /*   */ CCLASS_SPACE,
 /* ! */ CCLASS_OTHER,    /* " */ CCLASS_OTHER,   /* # */ CCLASS_OTHER,
 /* $ */ CCLASS_OTHER,    /* % */ CCLASS_OTHER,   /* & */ CCLASS_OTHER,
 /* ' */ CCLASS_OTHER,    /* ( */ CCLASS_OTHER,   /* ) */ CCLASS_OTHER,
 /* * */ CCLASS_OTHER,    /* + */ CCLASS_OTHER,   /* , */ CCLASS_OTHER,
 /* - */ CCLASS_OTHER,    /* . */ CCLASS_OTHER,   /* / */ CCLASS_OTHER,
 /* 0 */ CCLASS_DIGIT,    /* 1 */ CCLASS_DIGIT,   /* 2 */ CCLASS_DIGIT,
 /* 3 */ CCLASS_DIGIT,    /* 4 */ CCLASS_DIGIT,   /* 5 */ CCLASS_DIGIT,
 /* 6 */ CCLASS_DIGIT,    /* 7 */ CCLASS_DIGIT,   /* 8 */ CCLASS_DIGIT,
 /* 9 */ CCLASS_DIGIT,    /* : */ CCLASS_OTHER,   /* ; */ CCLASS_OTHER,
 /* < */ CCLASS_OTHER,    /* = */ CCLASS_OTHER,   /* > */ CCLASS_OTHER,
 /* ? */ CCLASS_OTHER,    /* @ */ CCLASS_OTHER,   /* A */ CCLASS_VOWEL,
 /* B */ CCLASS_B,        /* C */ CCLASS_C,       /* D */ CCLASS_D,
 /* E */ CCLASS_VOWEL,    /* F */ CCLASS_B,       /* G */ CCLASS_C,
 /* H */ CCLASS_SILENT,   /* I */ CCLASS_VOWEL,   /* J */ CCLASS_C,
 /* K */ CCLASS_C,        /* L */ CCLASS_L,       /* M */ CCLASS_M,
 /* N */ CCLASS_M,        /* O */ CCLASS_VOWEL,   /* P */ CCLASS_B,
 /* Q */ CCLASS_C,        /* R */ CCLASS_R,       /* S */ CCLASS_C,
 /* T */ CCLASS_D,        /* U */ CCLASS_VOWEL,   /* V */ CCLASS_B,
 /* W */ CCLASS_B,        /* X */ CCLASS_C,       /* Y */ CCLASS_Y,
 /* Z */ CCLASS_C,        /* [ */ CCLASS_OTHER,   /* \ */ CCLASS_OTHER,
 /* ] */ CCLASS_OTHER,    /* ^ */ CCLASS_OTHER,   /* _ */ CCLASS_OTHER,
 /* ` */ CCLASS_OTHER,    /* a */ CCLASS_VOWEL,   /* b */ CCLASS_B,
 /* c */ CCLASS_C,        /* d */ CCLASS_D,       /* e */ CCLASS_VOWEL,
 /* f */ CCLASS_B,        /* g */ CCLASS_C,       /* h */ CCLASS_SILENT,
 /* i */ CCLASS_VOWEL,    /* j */ CCLASS_C,       /* k */ CCLASS_C,
 /* l */ CCLASS_L,        /* m */ CCLASS_M,       /* n */ CCLASS_M,
 /* o */ CCLASS_VOWEL,    /* p */ CCLASS_B,       /* q */ CCLASS_C,
 /* r */ CCLASS_R,        /* s */ CCLASS_C,       /* t */ CCLASS_D,
 /* u */ CCLASS_VOWEL,    /* v */ CCLASS_B,       /* w */ CCLASS_B,
 /* x */ CCLASS_C,        /* y */ CCLASS_Y,       /* z */ CCLASS_C,
 /* { */ CCLASS_OTHER,    /* | */ CCLASS_OTHER,   /* } */ CCLASS_OTHER,
 /* ~ */ CCLASS_OTHER,    /*   */ CCLASS_OTHER,   
};

/*
** Mapping from the character class number (0-13) to a symbol for each
** character class.  Note that initClass[] can be used to map the class
** symbol back into the class number.
*/
static const unsigned char className[] = ".ABCDHLRMY9 ?";

/*
** Generate a "phonetic hash" from a string of ASCII characters
** in zIn[0..nIn-1].
**
**   * Map characters by character class as defined above.
**   * Omit double-letters
**   * Omit vowels beside R and L
**   * Omit T when followed by CH
**   * Omit W when followed by R
**   * Omit D when followed by J or G
**   * Omit K in KN or G in GN at the beginning of a word
**
** Space to hold the result is obtained from sqlite3_malloc()
**
** Return NULL if memory allocation fails.  
*/
static unsigned char *phoneticHash(const unsigned char *zIn, int nIn){
  unsigned char *zOut = sqlite3_malloc64( nIn + 1 );
  int i;
  int nOut = 0;
  char cPrev = 0x77;
  char cPrevX = 0x77;
  const unsigned char *aClass = initClass;

  if( zOut==0 ) return 0;
  if( nIn>2 ){
    switch( zIn[0] ){
      case 'g': 
      case 'k': {
        if( zIn[1]=='n' ){ zIn++; nIn--; }
        break;
      }
    }
  }
  for(i=0; i<nIn; i++){
    unsigned char c = zIn[i];
    if( i+1<nIn ){
      if( c=='w' && zIn[i+1]=='r' ) continue;
      if( c=='d' && (zIn[i+1]=='j' || zIn[i+1]=='g') ) continue;
      if( i+2<nIn ){
        if( c=='t' && zIn[i+1]=='c' && zIn[i+2]=='h' ) continue;
      }
    }
    c = aClass[c&0x7f];
    if( c==CCLASS_SPACE ) continue;
    if( c==CCLASS_OTHER && cPrev!=CCLASS_DIGIT ) continue;
    aClass = midClass;
    if( c==CCLASS_VOWEL && (cPrevX==CCLASS_R || cPrevX==CCLASS_L) ){
       continue; /* No vowels beside L or R */ 
    }
    if( (c==CCLASS_R || c==CCLASS_L) && cPrevX==CCLASS_VOWEL ){
       nOut--;   /* No vowels beside L or R */
    }
    cPrev = c;
    if( c==CCLASS_SILENT ) continue;
    cPrevX = c;
    c = className[c];
    assert( nOut>=0 );
    if( nOut==0 || c!=zOut[nOut-1] ) zOut[nOut++] = c;
  }
  zOut[nOut] = 0;
  return zOut;
}

/*
** This is an SQL function wrapper around phoneticHash().  See
** the description of phoneticHash() for additional information.
*/
static void phoneticHashSqlFunc(
  sqlite3_context *context,
  int argc,
  sqlite3_value **argv
){
  const unsigned char *zIn;
  unsigned char *zOut;

  zIn = sqlite3_value_text(argv[0]);
  if( zIn==0 ) return;
  zOut = phoneticHash(zIn, sqlite3_value_bytes(argv[0]));
  if( zOut==0 ){
    sqlite3_result_error_nomem(context);
  }else{
    sqlite3_result_text(context, (char*)zOut, -1, sqlite3_free);
  }
}

/*
** Return the character class number for a character given its
** context.
*/
static char characterClass(char cPrev, char c){
  return cPrev==0 ? initClass[c&0x7f] : midClass[c&0x7f];
}

/*
** Return the cost of inserting or deleting character c immediately
** following character cPrev.  If cPrev==0, that means c is the first
** character of the word.
*/
static int insertOrDeleteCost(char cPrev, char c, char cNext){
  char classC = characterClass(cPrev, c);
  char classCprev;

  if( classC==CCLASS_SILENT ){
    /* Insert or delete "silent" characters such as H or W */
    return 1;
  }
  if( cPrev==c ){
    /* Repeated characters, or miss a repeat */
    return 10;
  }
  if( classC==CCLASS_VOWEL && (cPrev=='r' || cNext=='r') ){
    return 20;  /* Insert a vowel before or after 'r' */
  }
  classCprev = characterClass(cPrev, cPrev);
  if( classC==classCprev ){
    if( classC==CCLASS_VOWEL ){
      /* Remove or add a new vowel to a vowel cluster */
      return 15;
    }else{
      /* Remove or add a consonant not in the same class */
      return 50;
    }
  }

  /* any other character insertion or deletion */
  return 100;
}

/*
** Divide the insertion cost by this factor when appending to the
** end of the word.
*/
#define FINAL_INS_COST_DIV  4

/*
** Return the cost of substituting cTo in place of cFrom assuming
** the previous character is cPrev.  If cPrev==0 then cTo is the first
** character of the word.
*/
static int substituteCost(char cPrev, char cFrom, char cTo){
  char classFrom, classTo;
  if( cFrom==cTo ){
    /* Exact match */
    return 0;
  }
  if( cFrom==(cTo^0x20) && ((cTo>='A' && cTo<='Z') || (cTo>='a' && cTo<='z')) ){
    /* differ only in case */
    return 0;
  }
  classFrom = characterClass(cPrev, cFrom);
  classTo = characterClass(cPrev, cTo);
  if( classFrom==classTo ){
    /* Same character class */
    return 40;
  }
  if( classFrom>=CCLASS_B && classFrom<=CCLASS_Y
      && classTo>=CCLASS_B && classTo<=CCLASS_Y ){
    /* Convert from one consonant to another, but in a different class */
    return 75;
  }
  /* Any other subsitution */
  return 100;
}

/*
** Given two strings zA and zB which are pure ASCII, return the cost
** of transforming zA into zB.  If zA ends with '*' assume that it is
** a prefix of zB and give only minimal penalty for extra characters
** on the end of zB.
**
** Smaller numbers mean a closer match.
**
** Negative values indicate an error:
**    -1  One of the inputs is NULL
**    -2  Non-ASCII characters on input
**    -3  Unable to allocate memory 
**
** If pnMatch is not NULL, then *pnMatch is set to the number of bytes
** of zB that matched the pattern in zA. If zA does not end with a '*',
** then this value is always the number of bytes in zB (i.e. strlen(zB)).
** If zA does end in a '*', then it is the number of bytes in the prefix
** of zB that was deemed to match zA.
*/
static int editdist1(const char *zA, const char *zB, int *pnMatch){
  int nA, nB;            /* Number of characters in zA[] and zB[] */
  int xA, xB;            /* Loop counters for zA[] and zB[] */
  char cA = 0, cB;       /* Current character of zA and zB */
  char cAprev, cBprev;   /* Previous character of zA and zB */
  char cAnext, cBnext;   /* Next character in zA and zB */
  int d;                 /* North-west cost value */
  int dc = 0;            /* North-west character value */
  int res;               /* Final result */
  int *m;                /* The cost matrix */
  char *cx;              /* Corresponding character values */
  int *toFree = 0;       /* Malloced space */
  int nMatch = 0;
  int mStack[60+15];     /* Stack space to use if not too much is needed */

  /* Early out if either input is NULL */
  if( zA==0 || zB==0 ) return -1;

  /* Skip any common prefix */
  while( zA[0] && zA[0]==zB[0] ){ dc = zA[0]; zA++; zB++; nMatch++; }
  if( pnMatch ) *pnMatch = nMatch;
  if( zA[0]==0 && zB[0]==0 ) return 0;

#if 0
  printf("A=\"%s\" B=\"%s\" dc=%c\n", zA, zB, dc?dc:' ');
#endif

  /* Verify input strings and measure their lengths */
  for(nA=0; zA[nA]; nA++){
    if( zA[nA]&0x80 ) return -2;
  }
  for(nB=0; zB[nB]; nB++){
    if( zB[nB]&0x80 ) return -2;
  }

  /* Special processing if either string is empty */
  if( nA==0 ){
    cBprev = (char)dc;
    for(xB=res=0; (cB = zB[xB])!=0; xB++){
      res += insertOrDeleteCost(cBprev, cB, zB[xB+1])/FINAL_INS_COST_DIV;
      cBprev = cB;
    }
    return res;
  }
  if( nB==0 ){
    cAprev = (char)dc;
    for(xA=res=0; (cA = zA[xA])!=0; xA++){
      res += insertOrDeleteCost(cAprev, cA, zA[xA+1]);
      cAprev = cA;
    }
    return res;
  }

  /* A is a prefix of B */
  if( zA[0]=='*' && zA[1]==0 ) return 0;

  /* Allocate and initialize the Wagner matrix */
  if( nB<(sizeof(mStack)*4)/(sizeof(mStack[0])*5) ){
    m = mStack;
  }else{
    m = toFree = sqlite3_malloc64( (nB+1)*5*sizeof(m[0])/4 );
    if( m==0 ) return -3;
  }
  cx = (char*)&m[nB+1];

  /* Compute the Wagner edit distance */
  m[0] = 0;
  cx[0] = (char)dc;
  cBprev = (char)dc;
  for(xB=1; xB<=nB; xB++){
    cBnext = zB[xB];
    cB = zB[xB-1];
    cx[xB] = cB;
    m[xB] = m[xB-1] + insertOrDeleteCost(cBprev, cB, cBnext);
    cBprev = cB;
  }
  cAprev = (char)dc;
  for(xA=1; xA<=nA; xA++){
    int lastA = (xA==nA);
    cA = zA[xA-1];
    cAnext = zA[xA];
    if( cA=='*' && lastA ) break;
    d = m[0];
    dc = cx[0];
    m[0] = d + insertOrDeleteCost(cAprev, cA, cAnext);
    cBprev = 0;
    for(xB=1; xB<=nB; xB++){
      int totalCost, insCost, delCost, subCost, ncx;
      cB = zB[xB-1];
      cBnext = zB[xB];

      /* Cost to insert cB */
      insCost = insertOrDeleteCost(cx[xB-1], cB, cBnext);
      if( lastA ) insCost /= FINAL_INS_COST_DIV;

      /* Cost to delete cA */
      delCost = insertOrDeleteCost(cx[xB], cA, cBnext);

      /* Cost to substitute cA->cB */
      subCost = substituteCost(cx[xB-1], cA, cB);

      /* Best cost */
      totalCost = insCost + m[xB-1];
      ncx = cB;
      if( (delCost + m[xB])<totalCost ){
        totalCost = delCost + m[xB];
        ncx = cA;
      }
      if( (subCost + d)<totalCost ){
        totalCost = subCost + d;
      }

#if 0
      printf("%d,%d d=%4d u=%4d r=%4d dc=%c cA=%c cB=%c"
             " ins=%4d del=%4d sub=%4d t=%4d ncx=%c\n",
             xA, xB, d, m[xB], m[xB-1], dc?dc:' ', cA, cB,
             insCost, delCost, subCost, totalCost, ncx?ncx:' ');
#endif

      /* Update the matrix */
      d = m[xB];
      dc = cx[xB];
      m[xB] = totalCost;
      cx[xB] = (char)ncx;
      cBprev = cB;
    }
    cAprev = cA;
  }

  /* Free the wagner matrix and return the result */
  if( cA=='*' ){
    res = m[1];
    for(xB=1; xB<=nB; xB++){
      if( m[xB]<res ){
        res = m[xB];
        if( pnMatch ) *pnMatch = xB+nMatch;
      }
    }
  }else{
    res = m[nB];
    /* In the current implementation, pnMatch is always NULL if zA does
    ** not end in "*" */
    assert( pnMatch==0 );
  }
  sqlite3_free(toFree);
  return res;
}

/*
** Function:    editdist(A,B)
**
** Return the cost of transforming string A into string B.  Both strings
** must be pure ASCII text.  If A ends with '*' then it is assumed to be
** a prefix of B and extra characters on the end of B have minimal additional
** cost.
*/
static void editdistSqlFunc(
  sqlite3_context *context,
  int argc,
  sqlite3_value **argv
){
  int res = editdist1(
                    (const char*)sqlite3_value_text(argv[0]),
                    (const char*)sqlite3_value_text(argv[1]),
                    0);
  if( res<0 ){
    if( res==(-3) ){
      sqlite3_result_error_nomem(context);
    }else if( res==(-2) ){
      sqlite3_result_error(context, "non-ASCII input to editdist()", -1);
    }else{
      sqlite3_result_error(context, "NULL input to editdist()", -1);
    }
  }else{ 
    sqlite3_result_int(context, res);
  }
}

/* End of the fixed-cost edit distance implementation
******************************************************************************
*****************************************************************************
** Begin: Configurable cost unicode edit distance routines
*/
/* Forward declaration of structures */
typedef struct EditDist3Cost EditDist3Cost;
typedef struct EditDist3Config EditDist3Config;
typedef struct EditDist3Point EditDist3Point;
typedef struct EditDist3From EditDist3From;
typedef struct EditDist3FromString EditDist3FromString;
typedef struct EditDist3To EditDist3To;
typedef struct EditDist3ToString EditDist3ToString;
typedef struct EditDist3Lang EditDist3Lang;


/*
** An entry in the edit cost table
*/
struct EditDist3Cost {
  EditDist3Cost *pNext;     /* Next cost element */
  u8 nFrom;                 /* Number of bytes in aFrom */
  u8 nTo;                   /* Number of bytes in aTo */
  u16 iCost;                /* Cost of this transformation */
  char a[4]    ;            /* FROM string followed by TO string */
  /* Additional TO and FROM string bytes appended as necessary */
};

/*
** Edit costs for a particular language ID 
*/
struct EditDist3Lang {
  int iLang;             /* Language ID */
  int iInsCost;          /* Default insertion cost */
  int iDelCost;          /* Default deletion cost */
  int iSubCost;          /* Default substitution cost */
  EditDist3Cost *pCost;  /* Costs */
};


/*
** The default EditDist3Lang object, with default costs.
*/
static const EditDist3Lang editDist3Lang = { 0, 100, 100, 150, 0 };

/*
** Complete configuration
*/
struct EditDist3Config {
  int nLang;             /* Number of language IDs.  Size of a[] */
  EditDist3Lang *a;      /* One for each distinct language ID */
};

/*
** Extra information about each character in the FROM string.
*/
struct EditDist3From {
  int nSubst;              /* Number of substitution cost entries */
  int nDel;                /* Number of deletion cost entries */
  int nByte;               /* Number of bytes in this character */
  EditDist3Cost **apSubst; /* Array of substitution costs for this element */
  EditDist3Cost **apDel;   /* Array of deletion cost entries */
};

/*
** A precompiled FROM string.
*
** In the common case we expect the FROM string to be reused multiple times.
** In other words, the common case will be to measure the edit distance
** from a single origin string to multiple target strings.
*/
struct EditDist3FromString {
  char *z;                 /* The complete text of the FROM string */
  int n;                   /* Number of characters in the FROM string */
  int isPrefix;            /* True if ends with '*' character */
  EditDist3From *a;        /* Extra info about each char of the FROM string */
};

/*
** Extra information about each character in the TO string.
*/
struct EditDist3To {
  int nIns;                /* Number of insertion cost entries */
  int nByte;               /* Number of bytes in this character */
  EditDist3Cost **apIns;   /* Array of deletion cost entries */
};

/*
** A precompiled FROM string
*/
struct EditDist3ToString {
  char *z;                 /* The complete text of the TO string */
  int n;                   /* Number of characters in the TO string */
  EditDist3To *a;          /* Extra info about each char of the TO string */
};

/*
** Clear or delete an instance of the object that records all edit-distance
** weights.
*/
static void editDist3ConfigClear(EditDist3Config *p){
  int i;
  if( p==0 ) return;
  for(i=0; i<p->nLang; i++){
    EditDist3Cost *pCost, *pNext;
    pCost = p->a[i].pCost;
    while( pCost ){
      pNext = pCost->pNext;
      sqlite3_free(pCost);
      pCost = pNext;
    }
  }
  sqlite3_free(p->a);
  memset(p, 0, sizeof(*p));
}
static void editDist3ConfigDelete(void *pIn){
  EditDist3Config *p = (EditDist3Config*)pIn;
  editDist3ConfigClear(p);
  sqlite3_free(p);
}

/* Compare the FROM values of two EditDist3Cost objects, for sorting.
** Return negative, zero, or positive if the A is less than, equal to,
** or greater than B.
*/
static int editDist3CostCompare(EditDist3Cost *pA, EditDist3Cost *pB){
  int n = pA->nFrom;
  int rc;
  if( n>pB->nFrom ) n = pB->nFrom;
  rc = strncmp(pA->a, pB->a, n);
  if( rc==0 ) rc = pA->nFrom - pB->nFrom;
  return rc;
}

/*
** Merge together two sorted lists of EditDist3Cost objects, in order
** of increasing FROM.
*/
static EditDist3Cost *editDist3CostMerge(
  EditDist3Cost *pA,
  EditDist3Cost *pB
){
  EditDist3Cost *pHead = 0;
  EditDist3Cost **ppTail = &pHead;
  EditDist3Cost *p;
  while( pA && pB ){
    if( editDist3CostCompare(pA,pB)<=0 ){
      p = pA;
      pA = pA->pNext;
    }else{
      p = pB;
      pB = pB->pNext;
    }
    *ppTail = p;
    ppTail =  &p->pNext;
  }
  if( pA ){
    *ppTail = pA;
  }else{
    *ppTail = pB;
  }
  return pHead;
}

/*
** Sort a list of EditDist3Cost objects into order of increasing FROM
*/
static EditDist3Cost *editDist3CostSort(EditDist3Cost *pList){
  EditDist3Cost *ap[60], *p;
  int i;
  int mx = 0;
  ap[0] = 0;
  ap[1] = 0;
  while( pList ){
    p = pList;
    pList = p->pNext;
    p->pNext = 0;
    for(i=0; ap[i]; i++){
      p = editDist3CostMerge(ap[i],p);
      ap[i] = 0;
    }
    ap[i] = p;
    if( i>mx ){
      mx = i;
      ap[i+1] = 0;
    }
  }
  p = 0;
  for(i=0; i<=mx; i++){
    if( ap[i] ) p = editDist3CostMerge(p,ap[i]);
  }
  return p;
}

/*
** Load all edit-distance weights from a table.
*/
static int editDist3ConfigLoad(
  EditDist3Config *p,      /* The edit distance configuration to load */
  sqlite3 *db,            /* Load from this database */
  const char *zTable      /* Name of the table from which to load */
){
  sqlite3_stmt *pStmt;
  int rc, rc2;
  char *zSql;
  int iLangPrev = -9999;
  EditDist3Lang *pLang = 0;

  zSql = sqlite3_mprintf("SELECT iLang, cFrom, cTo, iCost"
                         " FROM \"%w\" WHERE iLang>=0 ORDER BY iLang", zTable);
  if( zSql==0 ) return SQLITE_NOMEM;
  rc = sqlite3_prepare(db, zSql, -1, &pStmt, 0);
  sqlite3_free(zSql);
  if( rc ) return rc;
  editDist3ConfigClear(p);
  while( sqlite3_step(pStmt)==SQLITE_ROW ){
    int iLang = sqlite3_column_int(pStmt, 0);
    const char *zFrom = (const char*)sqlite3_column_text(pStmt, 1);
    int nFrom = zFrom ? sqlite3_column_bytes(pStmt, 1) : 0;
    const char *zTo = (const char*)sqlite3_column_text(pStmt, 2);
    int nTo = zTo ? sqlite3_column_bytes(pStmt, 2) : 0;
    int iCost = sqlite3_column_int(pStmt, 3);

    assert( zFrom!=0 || nFrom==0 );
    assert( zTo!=0 || nTo==0 );
    if( nFrom>100 || nTo>100 ) continue;
    if( iCost<0 ) continue;
    if( iCost>=10000 ) continue;  /* Costs above 10K are considered infinite */
    if( pLang==0 || iLang!=iLangPrev ){
      EditDist3Lang *pNew;
      pNew = sqlite3_realloc64(p->a, (p->nLang+1)*sizeof(p->a[0]));
      if( pNew==0 ){ rc = SQLITE_NOMEM; break; }
      p->a = pNew;
      pLang = &p->a[p->nLang];
      p->nLang++;
      pLang->iLang = iLang;
      pLang->iInsCost = 100;
      pLang->iDelCost = 100;
      pLang->iSubCost = 150;
      pLang->pCost = 0;
      iLangPrev = iLang;
    }
    if( nFrom==1 && zFrom[0]=='?' && nTo==0 ){
      pLang->iDelCost = iCost;
    }else if( nFrom==0 && nTo==1 && zTo[0]=='?' ){
      pLang->iInsCost = iCost;
    }else if( nFrom==1 && nTo==1 && zFrom[0]=='?' && zTo[0]=='?' ){
      pLang->iSubCost = iCost;
    }else{
      EditDist3Cost *pCost;
      int nExtra = nFrom + nTo - 4;
      if( nExtra<0 ) nExtra = 0;
      pCost = sqlite3_malloc64( sizeof(*pCost) + nExtra );
      if( pCost==0 ){ rc = SQLITE_NOMEM; break; }
      pCost->nFrom = (u8)nFrom;
      pCost->nTo = (u8)nTo;
      pCost->iCost = (u16)iCost;
      memcpy(pCost->a, zFrom, nFrom);
      memcpy(pCost->a + nFrom, zTo, nTo);
      pCost->pNext = pLang->pCost;
      pLang->pCost = pCost; 
    }
  }
  rc2 = sqlite3_finalize(pStmt);
  if( rc==SQLITE_OK ) rc = rc2;
  if( rc==SQLITE_OK ){
    int iLang;
    for(iLang=0; iLang<p->nLang; iLang++){
      p->a[iLang].pCost = editDist3CostSort(p->a[iLang].pCost);
    }
  }
  return rc;
}

/*
** Return the length (in bytes) of a utf-8 character.  Or return a maximum
** of N.
*/
static int utf8Len(unsigned char c, int N){
  int len = 1;
  if( c>0x7f ){
    if( (c&0xe0)==0xc0 ){
      len = 2;
    }else if( (c&0xf0)==0xe0 ){
      len = 3;
    }else{
      len = 4;
    }
  }
  if( len>N ) len = N;
  return len;
}

/*
** Return TRUE (non-zero) if the To side of the given cost matches
** the given string.
*/
static int matchTo(EditDist3Cost *p, const char *z, int n){
  assert( n>0 );
  if( p->a[p->nFrom]!=z[0] ) return 0;
  if( p->nTo>n ) return 0;
  if( strncmp(p->a+p->nFrom, z, p->nTo)!=0 ) return 0;
  return 1;
}

/*
** Return TRUE (non-zero) if the From side of the given cost matches
** the given string.
*/
static int matchFrom(EditDist3Cost *p, const char *z, int n){
  assert( p->nFrom<=n );
  if( p->nFrom ){
    if( p->a[0]!=z[0] ) return 0;
    if( strncmp(p->a, z, p->nFrom)!=0 ) return 0;
  }
  return 1;
}

/*
** Return TRUE (non-zero) of the next FROM character and the next TO
** character are the same.
*/
static int matchFromTo(
  EditDist3FromString *pStr,  /* Left hand string */
  int n1,                     /* Index of comparison character on the left */
  const char *z2,             /* Right-handl comparison character */
  int n2                      /* Bytes remaining in z2[] */
){
  int b1 = pStr->a[n1].nByte;
  if( b1>n2 ) return 0;
  assert( b1>0 );
  if( pStr->z[n1]!=z2[0] ) return 0;
  if( strncmp(pStr->z+n1, z2, b1)!=0 ) return 0;
  return 1;
}

/*
** Delete an EditDist3FromString objecct
*/
static void editDist3FromStringDelete(EditDist3FromString *p){
  int i;
  if( p ){
    for(i=0; i<p->n; i++){
      sqlite3_free(p->a[i].apDel);
      sqlite3_free(p->a[i].apSubst);
    }
    sqlite3_free(p);
  }
}

/*
** Create a EditDist3FromString object.
*/
static EditDist3FromString *editDist3FromStringNew(
  const EditDist3Lang *pLang,
  const char *z,
  int n
){
  EditDist3FromString *pStr;
  EditDist3Cost *p;
  int i;

  if( z==0 ) return 0;
  if( n<0 ) n = (int)strlen(z);
  pStr = sqlite3_malloc64( sizeof(*pStr) + sizeof(pStr->a[0])*n + n + 1 );
  if( pStr==0 ) return 0;
  pStr->a = (EditDist3From*)&pStr[1];
  memset(pStr->a, 0, sizeof(pStr->a[0])*n);
  pStr->n = n;
  pStr->z = (char*)&pStr->a[n];
  memcpy(pStr->z, z, n+1);
  if( n && z[n-1]=='*' ){
    pStr->isPrefix = 1;
    n--;
    pStr->n--;
    pStr->z[n] = 0;
  }else{
    pStr->isPrefix = 0;
  }

  for(i=0; i<n; i++){
    EditDist3From *pFrom = &pStr->a[i];
    memset(pFrom, 0, sizeof(*pFrom));
    pFrom->nByte = utf8Len((unsigned char)z[i], n-i);
    for(p=pLang->pCost; p; p=p->pNext){
      EditDist3Cost **apNew;
      if( i+p->nFrom>n ) continue;
      if( matchFrom(p, z+i, n-i)==0 ) continue;
      if( p->nTo==0 ){
        apNew = sqlite3_realloc64(pFrom->apDel,
                                sizeof(*apNew)*(pFrom->nDel+1));
        if( apNew==0 ) break;
        pFrom->apDel = apNew;
        apNew[pFrom->nDel++] = p;
      }else{
        apNew = sqlite3_realloc64(pFrom->apSubst,
                                sizeof(*apNew)*(pFrom->nSubst+1));
        if( apNew==0 ) break;
        pFrom->apSubst = apNew;
        apNew[pFrom->nSubst++] = p;
      }
    }
    if( p ){
      editDist3FromStringDelete(pStr);
      pStr = 0;
      break;
    }
  }
  return pStr;
}

/*
** Update entry m[i] such that it is the minimum of its current value
** and m[j]+iCost.
*/
static void updateCost(
  unsigned int *m,
  int i,
  int j,
  int iCost
){
  unsigned int b;
  assert( iCost>=0 );
  assert( iCost<10000 );
  b = m[j] + iCost;
  if( b<m[i] ) m[i] = b;
}

/*
** How much stack space (int bytes) to use for Wagner matrix in 
** editDist3Core().  If more space than this is required, the entire
** matrix is taken from the heap.  To reduce the load on the memory
** allocator, make this value as large as practical for the
** architecture in use.
*/
#ifndef SQLITE_SPELLFIX_STACKALLOC_SZ
# define SQLITE_SPELLFIX_STACKALLOC_SZ  (1024)
#endif

/* Compute the edit distance between two strings.
**
** If an error occurs, return a negative number which is the error code.
**
** If pnMatch is not NULL, then *pnMatch is set to the number of characters
** (not bytes) in z2 that matched the search pattern in *pFrom. If pFrom does
** not contain the pattern for a prefix-search, then this is always the number
** of characters in z2. If pFrom does contain a prefix search pattern, then
** it is the number of characters in the prefix of z2 that was deemed to 
** match pFrom.
*/
static int editDist3Core(
  EditDist3FromString *pFrom,  /* The FROM string */
  const char *z2,              /* The TO string */
  int n2,                      /* Length of the TO string */
  const EditDist3Lang *pLang,  /* Edit weights for a particular language ID */
  int *pnMatch                 /* OUT: Characters in matched prefix */
){
  int k, n;
  int i1, b1;
  int i2, b2;
  EditDist3FromString f = *pFrom;
  EditDist3To *a2;
  unsigned int *m;
  unsigned int *pToFree;
  int szRow;
  EditDist3Cost *p;
  int res;
  sqlite3_uint64 nByte;
  unsigned int stackSpace[SQLITE_SPELLFIX_STACKALLOC_SZ/sizeof(unsigned int)];

  /* allocate the Wagner matrix and the aTo[] array for the TO string */
  n = (f.n+1)*(n2+1);
  n = (n+1)&~1;
  nByte = n*sizeof(m[0]) + sizeof(a2[0])*n2;
  if( nByte<=sizeof(stackSpace) ){
    m = stackSpace;
    pToFree = 0;
  }else{
    m = pToFree = sqlite3_malloc64( nByte );
    if( m==0 ) return -1;            /* Out of memory */
  }
  a2 = (EditDist3To*)&m[n];
  memset(a2, 0, sizeof(a2[0])*n2);

  /* Fill in the a1[] matrix for all characters of the TO string */
  for(i2=0; i2<n2; i2++){
    a2[i2].nByte = utf8Len((unsigned char)z2[i2], n2-i2);
    for(p=pLang->pCost; p; p=p->pNext){
      EditDist3Cost **apNew;
      if( p->nFrom>0 ) break;
      if( i2+p->nTo>n2 ) continue;
      if( p->a[0]>z2[i2] ) break;
      if( matchTo(p, z2+i2, n2-i2)==0 ) continue;
      a2[i2].nIns++;
      apNew = sqlite3_realloc64(a2[i2].apIns, sizeof(*apNew)*a2[i2].nIns);
      if( apNew==0 ){
        res = -1;  /* Out of memory */
        goto editDist3Abort;
      }
      a2[i2].apIns = apNew;
      a2[i2].apIns[a2[i2].nIns-1] = p;
    }
  }

  /* Prepare to compute the minimum edit distance */
  szRow = f.n+1;
  memset(m, 0x01, (n2+1)*szRow*sizeof(m[0]));
  m[0] = 0;

  /* First fill in the top-row of the matrix with FROM deletion costs */
  for(i1=0; i1<f.n; i1 += b1){
    b1 = f.a[i1].nByte;
    updateCost(m, i1+b1, i1, pLang->iDelCost);
    for(k=0; k<f.a[i1].nDel; k++){
      p = f.a[i1].apDel[k];
      updateCost(m, i1+p->nFrom, i1, p->iCost);
    }
  }

  /* Fill in all subsequent rows, top-to-bottom, left-to-right */
  for(i2=0; i2<n2; i2 += b2){
    int rx;      /* Starting index for current row */
    int rxp;     /* Starting index for previous row */
    b2 = a2[i2].nByte;
    rx = szRow*(i2+b2);
    rxp = szRow*i2;
    updateCost(m, rx, rxp, pLang->iInsCost);
    for(k=0; k<a2[i2].nIns; k++){
      p = a2[i2].apIns[k];
      updateCost(m, szRow*(i2+p->nTo), rxp, p->iCost);
    }
    for(i1=0; i1<f.n; i1+=b1){
      int cx;    /* Index of current cell */
      int cxp;   /* Index of cell immediately to the left */
      int cxd;   /* Index of cell to the left and one row above */
      int cxu;   /* Index of cell immediately above */
      b1 = f.a[i1].nByte;
      cxp = rx + i1;
      cx = cxp + b1;
      cxd = rxp + i1;
      cxu = cxd + b1;
      updateCost(m, cx, cxp, pLang->iDelCost);
      for(k=0; k<f.a[i1].nDel; k++){
        p = f.a[i1].apDel[k];
        updateCost(m, cxp+p->nFrom, cxp, p->iCost);
      }
      updateCost(m, cx, cxu, pLang->iInsCost);
      if( matchFromTo(&f, i1, z2+i2, n2-i2) ){
        updateCost(m, cx, cxd, 0);
      }
      updateCost(m, cx, cxd, pLang->iSubCost);
      for(k=0; k<f.a[i1].nSubst; k++){
        p = f.a[i1].apSubst[k];
        if( matchTo(p, z2+i2, n2-i2) ){
          updateCost(m, cxd+p->nFrom+szRow*p->nTo, cxd, p->iCost);
        }
      }
    }
  }

#if 0  /* Enable for debugging */
  printf("         ^");
  for(i1=0; i1<f.n; i1++) printf(" %c-%2x", f.z[i1], f.z[i1]&0xff);
  printf("\n   ^:");
  for(i1=0; i1<szRow; i1++){
    int v = m[i1];
    if( v>9999 ) printf(" ****");
    else         printf(" %4d", v);
  }
  printf("\n");
  for(i2=0; i2<n2; i2++){
    printf("%c-%02x:", z2[i2], z2[i2]&0xff);
    for(i1=0; i1<szRow; i1++){
      int v = m[(i2+1)*szRow+i1];
      if( v>9999 ) printf(" ****");
      else         printf(" %4d", v);
    }
    printf("\n");
  }
#endif

  /* Free memory allocations and return the result */
  res = (int)m[szRow*(n2+1)-1];
  n = n2;
  if( f.isPrefix ){
    for(i2=1; i2<=n2; i2++){
      int b = m[szRow*i2-1];
      if( b<=res ){ 
        res = b;
        n = i2 - 1;
      }
    }
  }
  if( pnMatch ){
    int nExtra = 0;
    for(k=0; k<n; k++){
      if( (z2[k] & 0xc0)==0x80 ) nExtra++;
    }
    *pnMatch = n - nExtra;
  }

editDist3Abort:
  for(i2=0; i2<n2; i2++) sqlite3_free(a2[i2].apIns);
  sqlite3_free(pToFree);
  return res;
}

/*
** Get an appropriate EditDist3Lang object.
*/
static const EditDist3Lang *editDist3FindLang(
  EditDist3Config *pConfig,
  int iLang
){
  int i;
  for(i=0; i<pConfig->nLang; i++){
    if( pConfig->a[i].iLang==iLang ) return &pConfig->a[i];
  }
  return &editDist3Lang;
}

/*
** Function:    editdist3(A,B,iLang)
**              editdist3(tablename)
**
** Return the cost of transforming string A into string B using edit
** weights for iLang.
**
** The second form loads edit weights into memory from a table.
*/
static void editDist3SqlFunc(
  sqlite3_context *context,
  int argc,
  sqlite3_value **argv
){
  EditDist3Config *pConfig = (EditDist3Config*)sqlite3_user_data(context);
  sqlite3 *db = sqlite3_context_db_handle(context);
  int rc;
  if( argc==1 ){
    const char *zTable = (const char*)sqlite3_value_text(argv[0]);
    rc = editDist3ConfigLoad(pConfig, db, zTable);
    if( rc ) sqlite3_result_error_code(context, rc);
  }else{
    const char *zA = (const char*)sqlite3_value_text(argv[0]);
    const char *zB = (const char*)sqlite3_value_text(argv[1]);
    int nA = sqlite3_value_bytes(argv[0]);
    int nB = sqlite3_value_bytes(argv[1]);
    int iLang = argc==3 ? sqlite3_value_int(argv[2]) : 0;
    const EditDist3Lang *pLang = editDist3FindLang(pConfig, iLang);
    EditDist3FromString *pFrom;
    int dist;

    pFrom = editDist3FromStringNew(pLang, zA, nA);
    if( pFrom==0 ){
      sqlite3_result_error_nomem(context);
      return;
    }
    dist = editDist3Core(pFrom, zB, nB, pLang, 0);
    editDist3FromStringDelete(pFrom);
    if( dist==(-1) ){
      sqlite3_result_error_nomem(context);
    }else{
      sqlite3_result_int(context, dist);
    }
  } 
}

/*
** Register the editDist3 function with SQLite
*/
static int editDist3Install(sqlite3 *db){
  int rc;
  EditDist3Config *pConfig = sqlite3_malloc64( sizeof(*pConfig) );
  if( pConfig==0 ) return SQLITE_NOMEM;
  memset(pConfig, 0, sizeof(*pConfig));
  rc = sqlite3_create_function_v2(db, "editdist3",
              2, SQLITE_UTF8|SQLITE_DETERMINISTIC, pConfig,
              editDist3SqlFunc, 0, 0, 0);
  if( rc==SQLITE_OK ){
    rc = sqlite3_create_function_v2(db, "editdist3",
                3, SQLITE_UTF8|SQLITE_DETERMINISTIC, pConfig,
                editDist3SqlFunc, 0, 0, 0);
  }
  if( rc==SQLITE_OK ){
    rc = sqlite3_create_function_v2(db, "editdist3",
                1, SQLITE_UTF8|SQLITE_DETERMINISTIC, pConfig,
                editDist3SqlFunc, 0, 0, editDist3ConfigDelete);
  }else{
    sqlite3_free(pConfig);
  }
  return rc;
}
/* End configurable cost unicode edit distance routines
******************************************************************************
******************************************************************************
** Begin transliterate unicode-to-ascii implementation
*/

#if !SQLITE_AMALGAMATION
/*
** This lookup table is used to help decode the first byte of
** a multi-byte UTF8 character.
*/
static const unsigned char sqlite3Utf8Trans1[] = {
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
  0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
  0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
  0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
  0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
  0x00, 0x01, 0x02, 0x03, 0x00, 0x01, 0x00, 0x00,
};
#endif

/*
** Return the value of the first UTF-8 character in the string.
*/
static int utf8Read(const unsigned char *z, int n, int *pSize){
  int c, i;

  /* All callers to this routine (in the current implementation)
  ** always have n>0. */
  if( NEVER(n==0) ){
    c = i = 0;
  }else{
    c = z[0];
    i = 1;
    if( c>=0xc0 ){
      c = sqlite3Utf8Trans1[c-0xc0];
      while( i<n && (z[i] & 0xc0)==0x80 ){
        c = (c<<6) + (0x3f & z[i++]);
      }
    }
  }
  *pSize = i;
  return c;
}

/*
** Return the number of characters in the utf-8 string in the nIn byte
** buffer pointed to by zIn.
*/
static int utf8Charlen(const char *zIn, int nIn){
  int i;
  int nChar = 0;
  for(i=0; i<nIn; nChar++){
    int sz;
    utf8Read((const unsigned char *)&zIn[i], nIn-i, &sz);
    i += sz;
  }
  return nChar;
}

typedef struct Transliteration Transliteration;
struct Transliteration {
 unsigned short int cFrom;
 unsigned char cTo0, cTo1, cTo2, cTo3;
#ifdef SQLITE_SPELLFIX_5BYTE_MAPPINGS
 unsigned char cTo4;
#endif
};

/*
** Table of translations from unicode characters into ASCII.
*/
static const Transliteration translit[] = {
  { 0x00A0,  0x20, 0x00, 0x00, 0x00 },  /*   to   */
  { 0x00B5,  0x75, 0x00, 0x00, 0x00 },  /* µ to u */
  { 0x00C0,  0x41, 0x00, 0x00, 0x00 },  /* À to A */
  { 0x00C1,  0x41, 0x00, 0x00, 0x00 },  /* Á to A */
  { 0x00C2,  0x41, 0x00, 0x00, 0x00 },  /* Â to A */
  { 0x00C3,  0x41, 0x00, 0x00, 0x00 },  /* Ã to A */
  { 0x00C4,  0x41, 0x65, 0x00, 0x00 },  /* Ä to Ae */
  { 0x00C5,  0x41, 0x61, 0x00, 0x00 },  /* Å to Aa */
  { 0x00C6,  0x41, 0x45, 0x00, 0x00 },  /* Æ to AE */
  { 0x00C7,  0x43, 0x00, 0x00, 0x00 },  /* Ç to C */
  { 0x00C8,  0x45, 0x00, 0x00, 0x00 },  /* È to E */
  { 0x00C9,  0x45, 0x00, 0x00, 0x00 },  /* É to E */
  { 0x00CA,  0x45, 0x00, 0x00, 0x00 },  /* Ê to E */
  { 0x00CB,  0x45, 0x00, 0x00, 0x00 },  /* Ë to E */
  { 0x00CC,  0x49, 0x00, 0x00, 0x00 },  /* Ì to I */
  { 0x00CD,  0x49, 0x00, 0x00, 0x00 },  /* Í to I */
  { 0x00CE,  0x49, 0x00, 0x00, 0x00 },  /* Î to I */
  { 0x00CF,  0x49, 0x00, 0x00, 0x00 },  /* Ï to I */
  { 0x00D0,  0x44, 0x00, 0x00, 0x00 },  /* Ð to D */
  { 0x00D1,  0x4E, 0x00, 0x00, 0x00 },  /* Ñ to N */
  { 0x00D2,  0x4F, 0x00, 0x00, 0x00 },  /* Ò to O */
  { 0x00D3,  0x4F, 0x00, 0x00, 0x00 },  /* Ó to O */
  { 0x00D4,  0x4F, 0x00, 0x00, 0x00 },  /* Ô to O */
  { 0x00D5,  0x4F, 0x00, 0x00, 0x00 },  /* Õ to O */
  { 0x00D6,  0x4F, 0x65, 0x00, 0x00 },  /* Ö to Oe */
  { 0x00D7,  0x78, 0x00, 0x00, 0x00 },  /* × to x */
  { 0x00D8,  0x4F, 0x00, 0x00, 0x00 },  /* Ø to O */
  { 0x00D9,  0x55, 0x00, 0x00, 0x00 },  /* Ù to U */
  { 0x00DA,  0x55, 0x00, 0x00, 0x00 },  /* Ú to U */
  { 0x00DB,  0x55, 0x00, 0x00, 0x00 },  /* Û to U */
  { 0x00DC,  0x55, 0x65, 0x00, 0x00 },  /* Ü to Ue */
  { 0x00DD,  0x59, 0x00, 0x00, 0x00 },  /* Ý to Y */
  { 0x00DE,  0x54, 0x68, 0x00, 0x00 },  /* Þ to Th */
  { 0x00DF,  0x73, 0x73, 0x00, 0x00 },  /* ß to ss */
  { 0x00E0,  0x61, 0x00, 0x00, 0x00 },  /* à to a */
  { 0x00E1,  0x61, 0x00, 0x00, 0x00 },  /* á to a */
  { 0x00E2,  0x61, 0x00, 0x00, 0x00 },  /* â to a */
  { 0x00E3,  0x61, 0x00, 0x00, 0x00 },  /* ã to a */
  { 0x00E4,  0x61, 0x65, 0x00, 0x00 },  /* ä to ae */
  { 0x00E5,  0x61, 0x61, 0x00, 0x00 },  /* å to aa */
  { 0x00E6,  0x61, 0x65, 0x00, 0x00 },  /* æ to ae */
  { 0x00E7,  0x63, 0x00, 0x00, 0x00 },  /* ç to c */
  { 0x00E8,  0x65, 0x00, 0x00, 0x00 },  /* è to e */
  { 0x00E9,  0x65, 0x00, 0x00, 0x00 },  /* é to e */
  { 0x00EA,  0x65, 0x00, 0x00, 0x00 },  /* ê to e */
  { 0x00EB,  0x65, 0x00, 0x00, 0x00 },  /* ë to e */
  { 0x00EC,  0x69, 0x00, 0x00, 0x00 },  /* ì to i */
  { 0x00ED,  0x69, 0x00, 0x00, 0x00 },  /* í to i */
  { 0x00EE,  0x69, 0x00, 0x00, 0x00 },  /* î to i */
  { 0x00EF,  0x69, 0x00, 0x00, 0x00 },  /* ï to i */
  { 0x00F0,  0x64, 0x00, 0x00, 0x00 },  /* ð to d */
  { 0x00F1,  0x6E, 0x00, 0x00, 0x00 },  /* ñ to n */
  { 0x00F2,  0x6F, 0x00, 0x00, 0x00 },  /* ò to o */
  { 0x00F3,  0x6F, 0x00, 0x00, 0x00 },  /* ó to o */
  { 0x00F4,  0x6F, 0x00, 0x00, 0x00 },  /* ô to o */
  { 0x00F5,  0x6F, 0x00, 0x00, 0x00 },  /* õ to o */
  { 0x00F6,  0x6F, 0x65, 0x00, 0x00 },  /* ö to oe */
  { 0x00F7,  0x3A, 0x00, 0x00, 0x00 },  /* ÷ to : */
  { 0x00F8,  0x6F, 0x00, 0x00, 0x00 },  /* ø to o */
  { 0x00F9,  0x75, 0x00, 0x00, 0x00 },  /* ù to u */
  { 0x00FA,  0x75, 0x00, 0x00, 0x00 },  /* ú to u */
  { 0x00FB,  0x75, 0x00, 0x00, 0x00 },  /* û to u */
  { 0x00FC,  0x75, 0x65, 0x00, 0x00 },  /* ü to ue */
  { 0x00FD,  0x79, 0x00, 0x00, 0x00 },  /* ý to y */
  { 0x00FE,  0x74, 0x68, 0x00, 0x00 },  /* þ to th */
  { 0x00FF,  0x79, 0x00, 0x00, 0x00 },  /* ÿ to y */
  { 0x0100,  0x41, 0x00, 0x00, 0x00 },  /* Ā to A */
  { 0x0101,  0x61, 0x00, 0x00, 0x00 },  /* ā to a */
  { 0x0102,  0x41, 0x00, 0x00, 0x00 },  /* Ă to A */
  { 0x0103,  0x61, 0x00, 0x00, 0x00 },  /* ă to a */
  { 0x0104,  0x41, 0x00, 0x00, 0x00 },  /* Ą to A */
  { 0x0105,  0x61, 0x00, 0x00, 0x00 },  /* ą to a */
  { 0x0106,  0x43, 0x00, 0x00, 0x00 },  /* Ć to C */
  { 0x0107,  0x63, 0x00, 0x00, 0x00 },  /* ć to c */
  { 0x0108,  0x43, 0x68, 0x00, 0x00 },  /* Ĉ to Ch */
  { 0x0109,  0x63, 0x68, 0x00, 0x00 },  /* ĉ to ch */
  { 0x010A,  0x43, 0x00, 0x00, 0x00 },  /* Ċ to C */
  { 0x010B,  0x63, 0x00, 0x00, 0x00 },  /* ċ to c */
  { 0x010C,  0x43, 0x00, 0x00, 0x00 },  /* Č to C */
  { 0x010D,  0x63, 0x00, 0x00, 0x00 },  /* č to c */
  { 0x010E,  0x44, 0x00, 0x00, 0x00 },  /* Ď to D */
  { 0x010F,  0x64, 0x00, 0x00, 0x00 },  /* ď to d */
  { 0x0110,  0x44, 0x00, 0x00, 0x00 },  /* Đ to D */
  { 0x0111,  0x64, 0x00, 0x00, 0x00 },  /* đ to d */
  { 0x0112,  0x45, 0x00, 0x00, 0x00 },  /* Ē to E */
  { 0x0113,  0x65, 0x00, 0x00, 0x00 },  /* ē to e */
  { 0x0114,  0x45, 0x00, 0x00, 0x00 },  /* Ĕ to E */
  { 0x0115,  0x65, 0x00, 0x00, 0x00 },  /* ĕ to e */
  { 0x0116,  0x45, 0x00, 0x00, 0x00 },  /* Ė to E */
  { 0x0117,  0x65, 0x00, 0x00, 0x00 },  /* ė to e */
  { 0x0118,  0x45, 0x00, 0x00, 0x00 },  /* Ę to E */
  { 0x0119,  0x65, 0x00, 0x00, 0x00 },  /* ę to e */
  { 0x011A,  0x45, 0x00, 0x00, 0x00 },  /* Ě to E */
  { 0x011B,  0x65, 0x00, 0x00, 0x00 },  /* ě to e */
  { 0x011C,  0x47, 0x68, 0x00, 0x00 },  /* Ĝ to Gh */
  { 0x011D,  0x67, 0x68, 0x00, 0x00 },  /* ĝ to gh */
  { 0x011E,  0x47, 0x00, 0x00, 0x00 },  /* Ğ to G */
  { 0x011F,  0x67, 0x00, 0x00, 0x00 },  /* ğ to g */
  { 0x0120,  0x47, 0x00, 0x00, 0x00 },  /* Ġ to G */
  { 0x0121,  0x67, 0x00, 0x00, 0x00 },  /* ġ to g */
  { 0x0122,  0x47, 0x00, 0x00, 0x00 },  /* Ģ to G */
  { 0x0123,  0x67, 0x00, 0x00, 0x00 },  /* ģ to g */
  { 0x0124,  0x48, 0x68, 0x00, 0x00 },  /* Ĥ to Hh */
  { 0x0125,  0x68, 0x68, 0x00, 0x00 },  /* ĥ to hh */
  { 0x0126,  0x48, 0x00, 0x00, 0x00 },  /* Ħ to H */
  { 0x0127,  0x68, 0x00, 0x00, 0x00 },  /* ħ to h */
  { 0x0128,  0x49, 0x00, 0x00, 0x00 },  /* Ĩ to I */
  { 0x0129,  0x69, 0x00, 0x00, 0x00 },  /* ĩ to i */
  { 0x012A,  0x49, 0x00, 0x00, 0x00 },  /* Ī to I */
  { 0x012B,  0x69, 0x00, 0x00, 0x00 },  /* ī to i */
  { 0x012C,  0x49, 0x00, 0x00, 0x00 },  /* Ĭ to I */
  { 0x012D,  0x69, 0x00, 0x00, 0x00 },  /* ĭ to i */
  { 0x012E,  0x49, 0x00, 0x00, 0x00 },  /* Į to I */
  { 0x012F,  0x69, 0x00, 0x00, 0x00 },  /* į to i */
  { 0x0130,  0x49, 0x00, 0x00, 0x00 },  /* İ to I */
  { 0x0131,  0x69, 0x00, 0x00, 0x00 },  /* ı to i */
  { 0x0132,  0x49, 0x4A, 0x00, 0x00 },  /* Ĳ to IJ */
  { 0x0133,  0x69, 0x6A, 0x00, 0x00 },  /* ĳ to ij */
  { 0x0134,  0x4A, 0x68, 0x00, 0x00 },  /* Ĵ to Jh */
  { 0x0135,  0x6A, 0x68, 0x00, 0x00 },  /* ĵ to jh */
  { 0x0136,  0x4B, 0x00, 0x00, 0x00 },  /* Ķ to K */
  { 0x0137,  0x6B, 0x00, 0x00, 0x00 },  /* ķ to k */
  { 0x0138,  0x6B, 0x00, 0x00, 0x00 },  /* ĸ to k */
  { 0x0139,  0x4C, 0x00, 0x00, 0x00 },  /* Ĺ to L */
  { 0x013A,  0x6C, 0x00, 0x00, 0x00 },  /* ĺ to l */
  { 0x013B,  0x4C, 0x00, 0x00, 0x00 },  /* Ļ to L */
  { 0x013C,  0x6C, 0x00, 0x00, 0x00 },  /* ļ to l */
  { 0x013D,  0x4C, 0x00, 0x00, 0x00 },  /* Ľ to L */
  { 0x013E,  0x6C, 0x00, 0x00, 0x00 },  /* ľ to l */
  { 0x013F,  0x4C, 0x2E, 0x00, 0x00 },  /* Ŀ to L. */
  { 0x0140,  0x6C, 0x2E, 0x00, 0x00 },  /* ŀ to l. */
  { 0x0141,  0x4C, 0x00, 0x00, 0x00 },  /* Ł to L */
  { 0x0142,  0x6C, 0x00, 0x00, 0x00 },  /* ł to l */
  { 0x0143,  0x4E, 0x00, 0x00, 0x00 },  /* Ń to N */
  { 0x0144,  0x6E, 0x00, 0x00, 0x00 },  /* ń to n */
  { 0x0145,  0x4E, 0x00, 0x00, 0x00 },  /* Ņ to N */
  { 0x0146,  0x6E, 0x00, 0x00, 0x00 },  /* ņ to n */
  { 0x0147,  0x4E, 0x00, 0x00, 0x00 },  /* Ň to N */
  { 0x0148,  0x6E, 0x00, 0x00, 0x00 },  /* ň to n */
  { 0x0149,  0x27, 0x6E, 0x00, 0x00 },  /* ŉ to 'n */
  { 0x014A,  0x4E, 0x47, 0x00, 0x00 },  /* Ŋ to NG */
  { 0x014B,  0x6E, 0x67, 0x00, 0x00 },  /* ŋ to ng */
  { 0x014C,  0x4F, 0x00, 0x00, 0x00 },  /* Ō to O */
  { 0x014D,  0x6F, 0x00, 0x00, 0x00 },  /* ō to o */
  { 0x014E,  0x4F, 0x00, 0x00, 0x00 },  /* Ŏ to O */
  { 0x014F,  0x6F, 0x00, 0x00, 0x00 },  /* ŏ to o */
  { 0x0150,  0x4F, 0x00, 0x00, 0x00 },  /* Ő to O */
  { 0x0151,  0x6F, 0x00, 0x00, 0x00 },  /* ő to o */
  { 0x0152,  0x4F, 0x45, 0x00, 0x00 },  /* Œ to OE */
  { 0x0153,  0x6F, 0x65, 0x00, 0x00 },  /* œ to oe */
  { 0x0154,  0x52, 0x00, 0x00, 0x00 },  /* Ŕ to R */
  { 0x0155,  0x72, 0x00, 0x00, 0x00 },  /* ŕ to r */
  { 0x0156,  0x52, 0x00, 0x00, 0x00 },  /* Ŗ to R */
  { 0x0157,  0x72, 0x00, 0x00, 0x00 },  /* ŗ to r */
  { 0x0158,  0x52, 0x00, 0x00, 0x00 },  /* Ř to R */
  { 0x0159,  0x72, 0x00, 0x00, 0x00 },  /* ř to r */
  { 0x015A,  0x53, 0x00, 0x00, 0x00 },  /* Ś to S */
  { 0x015B,  0x73, 0x00, 0x00, 0x00 },  /* ś to s */
  { 0x015C,  0x53, 0x68, 0x00, 0x00 },  /* Ŝ to Sh */
  { 0x015D,  0x73, 0x68, 0x00, 0x00 },  /* ŝ to sh */
  { 0x015E,  0x53, 0x00, 0x00, 0x00 },  /* Ş to S */
  { 0x015F,  0x73, 0x00, 0x00, 0x00 },  /* ş to s */
  { 0x0160,  0x53, 0x00, 0x00, 0x00 },  /* Š to S */
  { 0x0161,  0x73, 0x00, 0x00, 0x00 },  /* š to s */
  { 0x0162,  0x54, 0x00, 0x00, 0x00 },  /* Ţ to T */
  { 0x0163,  0x74, 0x00, 0x00, 0x00 },  /* ţ to t */
  { 0x0164,  0x54, 0x00, 0x00, 0x00 },  /* Ť to T */
  { 0x0165,  0x74, 0x00, 0x00, 0x00 },  /* ť to t */
  { 0x0166,  0x54, 0x00, 0x00, 0x00 },  /* Ŧ to T */
  { 0x0167,  0x74, 0x00, 0x00, 0x00 },  /* ŧ to t */
  { 0x0168,  0x55, 0x00, 0x00, 0x00 },  /* Ũ to U */
  { 0x0169,  0x75, 0x00, 0x00, 0x00 },  /* ũ to u */
  { 0x016A,  0x55, 0x00, 0x00, 0x00 },  /* Ū to U */
  { 0x016B,  0x75, 0x00, 0x00, 0x00 },  /* ū to u */
  { 0x016C,  0x55, 0x00, 0x00, 0x00 },  /* Ŭ to U */
  { 0x016D,  0x75, 0x00, 0x00, 0x00 },  /* ŭ to u */
  { 0x016E,  0x55, 0x00, 0x00, 0x00 },  /* Ů to U */
  { 0x016F,  0x75, 0x00, 0x00, 0x00 },  /* ů to u */
  { 0x0170,  0x55, 0x00, 0x00, 0x00 },  /* Ű to U */
  { 0x0171,  0x75, 0x00, 0x00, 0x00 },  /* ű to u */
  { 0x0172,  0x55, 0x00, 0x00, 0x00 },  /* Ų to U */
  { 0x0173,  0x75, 0x00, 0x00, 0x00 },  /* ų to u */
  { 0x0174,  0x57, 0x00, 0x00, 0x00 },  /* Ŵ to W */
  { 0x0175,  0x77, 0x00, 0x00, 0x00 },  /* ŵ to w */
  { 0x0176,  0x59, 0x00, 0x00, 0x00 },  /* Ŷ to Y */
  { 0x0177,  0x79, 0x00, 0x00, 0x00 },  /* ŷ to y */
  { 0x0178,  0x59, 0x00, 0x00, 0x00 },  /* Ÿ to Y */
  { 0x0179,  0x5A, 0x00, 0x00, 0x00 },  /* Ź to Z */
  { 0x017A,  0x7A, 0x00, 0x00, 0x00 },  /* ź to z */
  { 0x017B,  0x5A, 0x00, 0x00, 0x00 },  /* Ż to Z */
  { 0x017C,  0x7A, 0x00, 0x00, 0x00 },  /* ż to z */
  { 0x017D,  0x5A, 0x00, 0x00, 0x00 },  /* Ž to Z */
  { 0x017E,  0x7A, 0x00, 0x00, 0x00 },  /* ž to z */
  { 0x017F,  0x73, 0x00, 0x00, 0x00 },  /* ſ to s */
  { 0x0192,  0x66, 0x00, 0x00, 0x00 },  /* ƒ to f */
  { 0x0218,  0x53, 0x00, 0x00, 0x00 },  /* Ș to S */
  { 0x0219,  0x73, 0x00, 0x00, 0x00 },  /* ș to s */
  { 0x021A,  0x54, 0x00, 0x00, 0x00 },  /* Ț to T */
  { 0x021B,  0x74, 0x00, 0x00, 0x00 },  /* ț to t */
  { 0x0386,  0x41, 0x00, 0x00, 0x00 },  /* Ά to A */
  { 0x0388,  0x45, 0x00, 0x00, 0x00 },  /* Έ to E */
  { 0x0389,  0x49, 0x00, 0x00, 0x00 },  /* Ή to I */
  { 0x038A,  0x49, 0x00, 0x00, 0x00 },  /* Ί to I */
  { 0x038C,  0x4f, 0x00, 0x00, 0x00 },  /* Ό to O */
  { 0x038E,  0x59, 0x00, 0x00, 0x00 },  /* Ύ to Y */
  { 0x038F,  0x4f, 0x00, 0x00, 0x00 },  /* Ώ to O */
  { 0x0390,  0x69, 0x00, 0x00, 0x00 },  /* ΐ to i */
  { 0x0391,  0x41, 0x00, 0x00, 0x00 },  /* Α to A */
  { 0x0392,  0x42, 0x00, 0x00, 0x00 },  /* Β to B */
  { 0x0393,  0x47, 0x00, 0x00, 0x00 },  /* Γ to G */
  { 0x0394,  0x44, 0x00, 0x00, 0x00 },  /* Δ to D */
  { 0x0395,  0x45, 0x00, 0x00, 0x00 },  /* Ε to E */
  { 0x0396,  0x5a, 0x00, 0x00, 0x00 },  /* Ζ to Z */
  { 0x0397,  0x49, 0x00, 0x00, 0x00 },  /* Η to I */
  { 0x0398,  0x54, 0x68, 0x00, 0x00 },  /* Θ to Th */
  { 0x0399,  0x49, 0x00, 0x00, 0x00 },  /* Ι to I */
  { 0x039A,  0x4b, 0x00, 0x00, 0x00 },  /* Κ to K */
  { 0x039B,  0x4c, 0x00, 0x00, 0x00 },  /* Λ to L */
  { 0x039C,  0x4d, 0x00, 0x00, 0x00 },  /* Μ to M */
  { 0x039D,  0x4e, 0x00, 0x00, 0x00 },  /* Ν to N */
  { 0x039E,  0x58, 0x00, 0x00, 0x00 },  /* Ξ to X */
  { 0x039F,  0x4f, 0x00, 0x00, 0x00 },  /* Ο to O */
  { 0x03A0,  0x50, 0x00, 0x00, 0x00 },  /* Π to P */
  { 0x03A1,  0x52, 0x00, 0x00, 0x00 },  /* Ρ to R */
  { 0x03A3,  0x53, 0x00, 0x00, 0x00 },  /* Σ to S */
  { 0x03A4,  0x54, 0x00, 0x00, 0x00 },  /* Τ to T */
  { 0x03A5,  0x59, 0x00, 0x00, 0x00 },  /* Υ to Y */
  { 0x03A6,  0x46, 0x00, 0x00, 0x00 },  /* Φ to F */
  { 0x03A7,  0x43, 0x68, 0x00, 0x00 },  /* Χ to Ch */
  { 0x03A8,  0x50, 0x73, 0x00, 0x00 },  /* Ψ to Ps */
  { 0x03A9,  0x4f, 0x00, 0x00, 0x00 },  /* Ω to O */
  { 0x03AA,  0x49, 0x00, 0x00, 0x00 },  /* Ϊ to I */
  { 0x03AB,  0x59, 0x00, 0x00, 0x00 },  /* Ϋ to Y */
  { 0x03AC,  0x61, 0x00, 0x00, 0x00 },  /* ά to a */
  { 0x03AD,  0x65, 0x00, 0x00, 0x00 },  /* έ to e */
  { 0x03AE,  0x69, 0x00, 0x00, 0x00 },  /* ή to i */
  { 0x03AF,  0x69, 0x00, 0x00, 0x00 },  /* ί to i */
  { 0x03B1,  0x61, 0x00, 0x00, 0x00 },  /* α to a */
  { 0x03B2,  0x62, 0x00, 0x00, 0x00 },  /* β to b */
  { 0x03B3,  0x67, 0x00, 0x00, 0x00 },  /* γ to g */
  { 0x03B4,  0x64, 0x00, 0x00, 0x00 },  /* δ to d */
  { 0x03B5,  0x65, 0x00, 0x00, 0x00 },  /* ε to e */
  { 0x03B6,  0x7a, 0x00, 0x00, 0x00 },  /* ζ to z */
  { 0x03B7,  0x69, 0x00, 0x00, 0x00 },  /* η to i */
  { 0x03B8,  0x74, 0x68, 0x00, 0x00 },  /* θ to th */
  { 0x03B9,  0x69, 0x00, 0x00, 0x00 },  /* ι to i */
  { 0x03BA,  0x6b, 0x00, 0x00, 0x00 },  /* κ to k */
  { 0x03BB,  0x6c, 0x00, 0x00, 0x00 },  /* λ to l */
  { 0x03BC,  0x6d, 0x00, 0x00, 0x00 },  /* μ to m */
  { 0x03BD,  0x6e, 0x00, 0x00, 0x00 },  /* ν to n */
  { 0x03BE,  0x78, 0x00, 0x00, 0x00 },  /* ξ to x */
  { 0x03BF,  0x6f, 0x00, 0x00, 0x00 },  /* ο to o */
  { 0x03C0,  0x70, 0x00, 0x00, 0x00 },  /* π to p */
  { 0x03C1,  0x72, 0x00, 0x00, 0x00 },  /* ρ to r */
  { 0x03C3,  0x73, 0x00, 0x00, 0x00 },  /* σ to s */
  { 0x03C4,  0x74, 0x00, 0x00, 0x00 },  /* τ to t */
  { 0x03C5,  0x79, 0x00, 0x00, 0x00 },  /* υ to y */
  { 0x03C6,  0x66, 0x00, 0x00, 0x00 },  /* φ to f */
  { 0x03C7,  0x63, 0x68, 0x00, 0x00 },  /* χ to ch */
  { 0x03C8,  0x70, 0x73, 0x00, 0x00 },  /* ψ to ps */
  { 0x03C9,  0x6f, 0x00, 0x00, 0x00 },  /* ω to o */
  { 0x03CA,  0x69, 0x00, 0x00, 0x00 },  /* ϊ to i */
  { 0x03CB,  0x79, 0x00, 0x00, 0x00 },  /* ϋ to y */
  { 0x03CC,  0x6f, 0x00, 0x00, 0x00 },  /* ό to o */
  { 0x03CD,  0x79, 0x00, 0x00, 0x00 },  /* ύ to y */
  { 0x03CE,  0x69, 0x00, 0x00, 0x00 },  /* ώ to i */
  { 0x0400,  0x45, 0x00, 0x00, 0x00 },  /* Ѐ to E */
  { 0x0401,  0x45, 0x00, 0x00, 0x00 },  /* Ё to E */
  { 0x0402,  0x44, 0x00, 0x00, 0x00 },  /* Ђ to D */
  { 0x0403,  0x47, 0x00, 0x00, 0x00 },  /* Ѓ to G */
  { 0x0404,  0x45, 0x00, 0x00, 0x00 },  /* Є to E */
  { 0x0405,  0x5a, 0x00, 0x00, 0x00 },  /* Ѕ to Z */
  { 0x0406,  0x49, 0x00, 0x00, 0x00 },  /* І to I */
  { 0x0407,  0x49, 0x00, 0x00, 0x00 },  /* Ї to I */
  { 0x0408,  0x4a, 0x00, 0x00, 0x00 },  /* Ј to J */
  { 0x0409,  0x49, 0x00, 0x00, 0x00 },  /* Љ to I */
  { 0x040A,  0x4e, 0x00, 0x00, 0x00 },  /* Њ to N */
  { 0x040B,  0x44, 0x00, 0x00, 0x00 },  /* Ћ to D */
  { 0x040C,  0x4b, 0x00, 0x00, 0x00 },  /* Ќ to K */
  { 0x040D,  0x49, 0x00, 0x00, 0x00 },  /* Ѝ to I */
  { 0x040E,  0x55, 0x00, 0x00, 0x00 },  /* Ў to U */
  { 0x040F,  0x44, 0x00, 0x00, 0x00 },  /* Џ to D */
  { 0x0410,  0x41, 0x00, 0x00, 0x00 },  /* А to A */
  { 0x0411,  0x42, 0x00, 0x00, 0x00 },  /* Б to B */
  { 0x0412,  0x56, 0x00, 0x00, 0x00 },  /* В to V */
  { 0x0413,  0x47, 0x00, 0x00, 0x00 },  /* Г to G */
  { 0x0414,  0x44, 0x00, 0x00, 0x00 },  /* Д to D */
  { 0x0415,  0x45, 0x00, 0x00, 0x00 },  /* Е to E */
  { 0x0416,  0x5a, 0x68, 0x00, 0x00 },  /* Ж to Zh */
  { 0x0417,  0x5a, 0x00, 0x00, 0x00 },  /* З to Z */
  { 0x0418,  0x49, 0x00, 0x00, 0x00 },  /* И to I */
  { 0x0419,  0x49, 0x00, 0x00, 0x00 },  /* Й to I */
  { 0x041A,  0x4b, 0x00, 0x00, 0x00 },  /* К to K */
  { 0x041B,  0x4c, 0x00, 0x00, 0x00 },  /* Л to L */
  { 0x041C,  0x4d, 0x00, 0x00, 0x00 },  /* М to M */
  { 0x041D,  0x4e, 0x00, 0x00, 0x00 },  /* Н to N */
  { 0x041E,  0x4f, 0x00, 0x00, 0x00 },  /* О to O */
  { 0x041F,  0x50, 0x00, 0x00, 0x00 },  /* П to P */
  { 0x0420,  0x52, 0x00, 0x00, 0x00 },  /* Р to R */
  { 0x0421,  0x53, 0x00, 0x00, 0x00 },  /* С to S */
  { 0x0422,  0x54, 0x00, 0x00, 0x00 },  /* Т to T */
  { 0x0423,  0x55, 0x00, 0x00, 0x00 },  /* У to U */
  { 0x0424,  0x46, 0x00, 0x00, 0x00 },  /* Ф to F */
  { 0x0425,  0x4b, 0x68, 0x00, 0x00 },  /* Х to Kh */
  { 0x0426,  0x54, 0x63, 0x00, 0x00 },  /* Ц to Tc */
  { 0x0427,  0x43, 0x68, 0x00, 0x00 },  /* Ч to Ch */
  { 0x0428,  0x53, 0x68, 0x00, 0x00 },  /* Ш to Sh */
  { 0x0429,  0x53, 0x68, 0x63, 0x68 },  /* Щ to Shch */
  { 0x042A,  0x61, 0x00, 0x00, 0x00 },  /*  to A */
  { 0x042B,  0x59, 0x00, 0x00, 0x00 },  /* Ы to Y */
  { 0x042C,  0x59, 0x00, 0x00, 0x00 },  /*  to Y */
  { 0x042D,  0x45, 0x00, 0x00, 0x00 },  /* Э to E */
  { 0x042E,  0x49, 0x75, 0x00, 0x00 },  /* Ю to Iu */
  { 0x042F,  0x49, 0x61, 0x00, 0x00 },  /* Я to Ia */
  { 0x0430,  0x61, 0x00, 0x00, 0x00 },  /* а to a */
  { 0x0431,  0x62, 0x00, 0x00, 0x00 },  /* б to b */
  { 0x0432,  0x76, 0x00, 0x00, 0x00 },  /* в to v */
  { 0x0433,  0x67, 0x00, 0x00, 0x00 },  /* г to g */
  { 0x0434,  0x64, 0x00, 0x00, 0x00 },  /* д to d */
  { 0x0435,  0x65, 0x00, 0x00, 0x00 },  /* е to e */
  { 0x0436,  0x7a, 0x68, 0x00, 0x00 },  /* ж to zh */
  { 0x0437,  0x7a, 0x00, 0x00, 0x00 },  /* з to z */
  { 0x0438,  0x69, 0x00, 0x00, 0x00 },  /* и to i */
  { 0x0439,  0x69, 0x00, 0x00, 0x00 },  /* й to i */
  { 0x043A,  0x6b, 0x00, 0x00, 0x00 },  /* к to k */
  { 0x043B,  0x6c, 0x00, 0x00, 0x00 },  /* л to l */
  { 0x043C,  0x6d, 0x00, 0x00, 0x00 },  /* м to m */
  { 0x043D,  0x6e, 0x00, 0x00, 0x00 },  /* н to n */
  { 0x043E,  0x6f, 0x00, 0x00, 0x00 },  /* о to o */
  { 0x043F,  0x70, 0x00, 0x00, 0x00 },  /* п to p */
  { 0x0440,  0x72, 0x00, 0x00, 0x00 },  /* р to r */
  { 0x0441,  0x73, 0x00, 0x00, 0x00 },  /* с to s */
  { 0x0442,  0x74, 0x00, 0x00, 0x00 },  /* т to t */
  { 0x0443,  0x75, 0x00, 0x00, 0x00 },  /* у to u */
  { 0x0444,  0x66, 0x00, 0x00, 0x00 },  /* ф to f */
  { 0x0445,  0x6b, 0x68, 0x00, 0x00 },  /* х to kh */
  { 0x0446,  0x74, 0x63, 0x00, 0x00 },  /* ц to tc */
  { 0x0447,  0x63, 0x68, 0x00, 0x00 },  /* ч to ch */
  { 0x0448,  0x73, 0x68, 0x00, 0x00 },  /* ш to sh */
  { 0x0449,  0x73, 0x68, 0x63, 0x68 },  /* щ to shch */
  { 0x044A,  0x61, 0x00, 0x00, 0x00 },  /*  to a */
  { 0x044B,  0x79, 0x00, 0x00, 0x00 },  /* ы to y */
  { 0x044C,  0x79, 0x00, 0x00, 0x00 },  /*  to y */
  { 0x044D,  0x65, 0x00, 0x00, 0x00 },  /* э to e */
  { 0x044E,  0x69, 0x75, 0x00, 0x00 },  /* ю to iu */
  { 0x044F,  0x69, 0x61, 0x00, 0x00 },  /* я to ia */
  { 0x0450,  0x65, 0x00, 0x00, 0x00 },  /* ѐ to e */
  { 0x0451,  0x65, 0x00, 0x00, 0x00 },  /* ё to e */
  { 0x0452,  0x64, 0x00, 0x00, 0x00 },  /* ђ to d */
  { 0x0453,  0x67, 0x00, 0x00, 0x00 },  /* ѓ to g */
  { 0x0454,  0x65, 0x00, 0x00, 0x00 },  /* є to e */
  { 0x0455,  0x7a, 0x00, 0x00, 0x00 },  /* ѕ to z */
  { 0x0456,  0x69, 0x00, 0x00, 0x00 },  /* і to i */
  { 0x0457,  0x69, 0x00, 0x00, 0x00 },  /* ї to i */
  { 0x0458,  0x6a, 0x00, 0x00, 0x00 },  /* ј to j */
  { 0x0459,  0x69, 0x00, 0x00, 0x00 },  /* љ to i */
  { 0x045A,  0x6e, 0x00, 0x00, 0x00 },  /* њ to n */
  { 0x045B,  0x64, 0x00, 0x00, 0x00 },  /* ћ to d */
  { 0x045C,  0x6b, 0x00, 0x00, 0x00 },  /* ќ to k */
  { 0x045D,  0x69, 0x00, 0x00, 0x00 },  /* ѝ to i */
  { 0x045E,  0x75, 0x00, 0x00, 0x00 },  /* ў to u */
  { 0x045F,  0x64, 0x00, 0x00, 0x00 },  /* џ to d */
  { 0x1E02,  0x42, 0x00, 0x00, 0x00 },  /* Ḃ to B */
  { 0x1E03,  0x62, 0x00, 0x00, 0x00 },  /* ḃ to b */
  { 0x1E0A,  0x44, 0x00, 0x00, 0x00 },  /* Ḋ to D */
  { 0x1E0B,  0x64, 0x00, 0x00, 0x00 },  /* ḋ to d */
  { 0x1E1E,  0x46, 0x00, 0x00, 0x00 },  /* Ḟ to F */
  { 0x1E1F,  0x66, 0x00, 0x00, 0x00 },  /* ḟ to f */
  { 0x1E40,  0x4D, 0x00, 0x00, 0x00 },  /* Ṁ to M */
  { 0x1E41,  0x6D, 0x00, 0x00, 0x00 },  /* ṁ to m */
  { 0x1E56,  0x50, 0x00, 0x00, 0x00 },  /* Ṗ to P */
  { 0x1E57,  0x70, 0x00, 0x00, 0x00 },  /* ṗ to p */
  { 0x1E60,  0x53, 0x00, 0x00, 0x00 },  /* Ṡ to S */
  { 0x1E61,  0x73, 0x00, 0x00, 0x00 },  /* ṡ to s */
  { 0x1E6A,  0x54, 0x00, 0x00, 0x00 },  /* Ṫ to T */
  { 0x1E6B,  0x74, 0x00, 0x00, 0x00 },  /* ṫ to t */
  { 0x1E80,  0x57, 0x00, 0x00, 0x00 },  /* Ẁ to W */
  { 0x1E81,  0x77, 0x00, 0x00, 0x00 },  /* ẁ to w */
  { 0x1E82,  0x57, 0x00, 0x00, 0x00 },  /* Ẃ to W */
  { 0x1E83,  0x77, 0x00, 0x00, 0x00 },  /* ẃ to w */
  { 0x1E84,  0x57, 0x00, 0x00, 0x00 },  /* Ẅ to W */
  { 0x1E85,  0x77, 0x00, 0x00, 0x00 },  /* ẅ to w */
  { 0x1EF2,  0x59, 0x00, 0x00, 0x00 },  /* Ỳ to Y */
  { 0x1EF3,  0x79, 0x00, 0x00, 0x00 },  /* ỳ to y */
  { 0xFB00,  0x66, 0x66, 0x00, 0x00 },  /* ﬀ to ff */
  { 0xFB01,  0x66, 0x69, 0x00, 0x00 },  /* ﬁ to fi */
  { 0xFB02,  0x66, 0x6C, 0x00, 0x00 },  /* ﬂ to fl */
  { 0xFB05,  0x73, 0x74, 0x00, 0x00 },  /* ﬅ to st */
  { 0xFB06,  0x73, 0x74, 0x00, 0x00 },  /* ﬆ to st */
};

static const Transliteration *spellfixFindTranslit(int c, int *pxTop){
  *pxTop = (sizeof(translit)/sizeof(translit[0])) - 1;
  return translit;
}

/*
** Convert the input string from UTF-8 into pure ASCII by converting
** all non-ASCII characters to some combination of characters in the
** ASCII subset.
**
** The returned string might contain more characters than the input.
**
** Space to hold the returned string comes from sqlite3_malloc() and
** should be freed by the caller.
*/
static unsigned char *transliterate(const unsigned char *zIn, int nIn){
#ifdef SQLITE_SPELLFIX_5BYTE_MAPPINGS
  unsigned char *zOut = sqlite3_malloc64( nIn*5 + 1 );
#else
  unsigned char *zOut = sqlite3_malloc64( nIn*4 + 1 );
#endif
  int c, sz, nOut;
  if( zOut==0 ) return 0;
  nOut = 0;
  while( nIn>0 ){
    c = utf8Read(zIn, nIn, &sz);
    zIn += sz;
    nIn -= sz;
    if( c<=127 ){
      zOut[nOut++] = (unsigned char)c;
    }else{
      int xTop, xBtm, x;
      const Transliteration *tbl = spellfixFindTranslit(c, &xTop);
      xBtm = 0;
      while( xTop>=xBtm ){
        x = (xTop + xBtm)/2;
        if( tbl[x].cFrom==c ){
          zOut[nOut++] = tbl[x].cTo0;
          if( tbl[x].cTo1 ){
            zOut[nOut++] = tbl[x].cTo1;
            if( tbl[x].cTo2 ){
              zOut[nOut++] = tbl[x].cTo2;
              if( tbl[x].cTo3 ){
                zOut[nOut++] = tbl[x].cTo3;
#ifdef SQLITE_SPELLFIX_5BYTE_MAPPINGS
                if( tbl[x].cTo4 ){
                  zOut[nOut++] = tbl[x].cTo4;
                }
#endif /* SQLITE_SPELLFIX_5BYTE_MAPPINGS */
              }
            }
          }
          c = 0;
          break;
        }else if( tbl[x].cFrom>c ){
          xTop = x-1;
        }else{
          xBtm = x+1;
        }
      }
      if( c ) zOut[nOut++] = '?';
    }
  }
  zOut[nOut] = 0;
  return zOut;
}

/*
** Return the number of characters in the shortest prefix of the input
** string that transliterates to an ASCII string nTrans bytes or longer.
** Or, if the transliteration of the input string is less than nTrans
** bytes in size, return the number of characters in the input string.
*/
static int translen_to_charlen(const char *zIn, int nIn, int nTrans){
  int i, c, sz, nOut;
  int nChar;

  i = nOut = 0;
  for(nChar=0; i<nIn && nOut<nTrans; nChar++){
    c = utf8Read((const unsigned char *)&zIn[i], nIn-i, &sz);
    i += sz;

    nOut++;
    if( c>=128 ){
      int xTop, xBtm, x;
      const Transliteration *tbl = spellfixFindTranslit(c, &xTop);
      xBtm = 0;
      while( xTop>=xBtm ){
        x = (xTop + xBtm)/2;
        if( tbl[x].cFrom==c ){
          if( tbl[x].cTo1 ){
            nOut++;
            if( tbl[x].cTo2 ){
              nOut++;
              if( tbl[x].cTo3 ){
                nOut++;
              }
            }
          }
          break;
        }else if( tbl[x].cFrom>c ){
          xTop = x-1;
        }else{
          xBtm = x+1;
        }
      }
    }
  }

  return nChar;
}


/*
**    spellfix1_translit(X)
**
** Convert a string that contains non-ASCII Roman characters into 
** pure ASCII.
*/
static void transliterateSqlFunc(
  sqlite3_context *context,
  int argc,
  sqlite3_value **argv
){
  const unsigned char *zIn = sqlite3_value_text(argv[0]);
  int nIn = sqlite3_value_bytes(argv[0]);
  unsigned char *zOut = transliterate(zIn, nIn);
  if( zOut==0 ){
    sqlite3_result_error_nomem(context);
  }else{
    sqlite3_result_text(context, (char*)zOut, -1, sqlite3_free);
  }
}

/*
**    spellfix1_scriptcode(X)
**
** Try to determine the dominant script used by the word X and return
** its ISO 15924 numeric code.
**
** The current implementation only understands the following scripts:
**
**    215  (Latin)
**    220  (Cyrillic)
**    200  (Greek)
**
** This routine will return 998 if the input X contains characters from
** two or more of the above scripts or 999 if X contains no characters
** from any of the above scripts.
*/
static void scriptCodeSqlFunc(
  sqlite3_context *context,
  int argc,
  sqlite3_value **argv
){
  const unsigned char *zIn = sqlite3_value_text(argv[0]);
  int nIn = sqlite3_value_bytes(argv[0]);
  int c, sz;
  int scriptMask = 0;
  int res;
  int seenDigit = 0;
# define SCRIPT_LATIN       0x0001
# define SCRIPT_CYRILLIC    0x0002
# define SCRIPT_GREEK       0x0004
# define SCRIPT_HEBREW      0x0008
# define SCRIPT_ARABIC      0x0010

  while( nIn>0 ){
    c = utf8Read(zIn, nIn, &sz);
    zIn += sz;
    nIn -= sz;
    if( c<0x02af ){
      if( c>=0x80 || midClass[c&0x7f]<CCLASS_DIGIT ){
        scriptMask |= SCRIPT_LATIN;
      }else if( c>='0' && c<='9' ){
        seenDigit = 1;
      }
    }else if( c>=0x0400 && c<=0x04ff ){
      scriptMask |= SCRIPT_CYRILLIC;
    }else if( c>=0x0386 && c<=0x03ce ){
      scriptMask |= SCRIPT_GREEK;
    }else if( c>=0x0590 && c<=0x05ff ){
      scriptMask |= SCRIPT_HEBREW;
    }else if( c>=0x0600 && c<=0x06ff ){
      scriptMask |= SCRIPT_ARABIC;
    }
  }
  if( scriptMask==0 && seenDigit ) scriptMask = SCRIPT_LATIN;
  switch( scriptMask ){
    case 0:                res = 999; break;
    case SCRIPT_LATIN:     res = 215; break;
    case SCRIPT_CYRILLIC:  res = 220; break;
    case SCRIPT_GREEK:     res = 200; break;
    case SCRIPT_HEBREW:    res = 125; break;
    case SCRIPT_ARABIC:    res = 160; break;
    default:               res = 998; break;
  }
  sqlite3_result_int(context, res);
}

/* End transliterate
******************************************************************************
******************************************************************************
** Begin spellfix1 virtual table.
*/

/* Maximum length of a phonehash used for querying the shadow table */
#define SPELLFIX_MX_HASH  32

/* Maximum number of hash strings to examine per query */
#define SPELLFIX_MX_RUN   1

typedef struct spellfix1_vtab spellfix1_vtab;
typedef struct spellfix1_cursor spellfix1_cursor;

/* Fuzzy-search virtual table object */
struct spellfix1_vtab {
  sqlite3_vtab base;         /* Base class - must be first */
  sqlite3 *db;               /* Database connection */
  char *zDbName;             /* Name of database holding this table */
  char *zTableName;          /* Name of the virtual table */
  char *zCostTable;          /* Table holding edit-distance cost numbers */
  EditDist3Config *pConfig3; /* Parsed edit distance costs */
};

/* Fuzzy-search cursor object */
struct spellfix1_cursor {
  sqlite3_vtab_cursor base;    /* Base class - must be first */
  spellfix1_vtab *pVTab;       /* The table to which this cursor belongs */
  char *zPattern;              /* rhs of MATCH clause */
  int idxNum;                  /* idxNum value passed to xFilter() */
  int nRow;                    /* Number of rows of content */
  int nAlloc;                  /* Number of allocated rows */
  int iRow;                    /* Current row of content */
  int iLang;                   /* Value of the langid= constraint */
  int iTop;                    /* Value of the top= constraint */
  int iScope;                  /* Value of the scope= constraint */
  int nSearch;                 /* Number of vocabulary items checked */
  sqlite3_stmt *pFullScan;     /* Shadow query for a full table scan */
  struct spellfix1_row {       /* For each row of content */
    sqlite3_int64 iRowid;         /* Rowid for this row */
    char *zWord;                  /* Text for this row */
    int iRank;                    /* Rank for this row */
    int iDistance;                /* Distance from pattern for this row */
    int iScore;                   /* Score for sorting */
    int iMatchlen;                /* Value of matchlen column (or -1) */
    char zHash[SPELLFIX_MX_HASH]; /* the phonehash used for this match */
  } *a; 
};

/*
** Construct one or more SQL statements from the format string given
** and then evaluate those statements. The success code is written
** into *pRc.
**
** If *pRc is initially non-zero then this routine is a no-op.
*/
static void spellfix1DbExec(
  int *pRc,              /* Success code */
  sqlite3 *db,           /* Database in which to run SQL */
  const char *zFormat,   /* Format string for SQL */
  ...                    /* Arguments to the format string */
){
  va_list ap;
  char *zSql;
  if( *pRc ) return;
  va_start(ap, zFormat);
  zSql = sqlite3_vmprintf(zFormat, ap);
  va_end(ap);
  if( zSql==0 ){
    *pRc = SQLITE_NOMEM;
  }else{
    *pRc = sqlite3_exec(db, zSql, 0, 0, 0);
    sqlite3_free(zSql);
  }
}

/*
** xDisconnect/xDestroy method for the fuzzy-search module.
*/
static int spellfix1Uninit(int isDestroy, sqlite3_vtab *pVTab){
  spellfix1_vtab *p = (spellfix1_vtab*)pVTab;
  int rc = SQLITE_OK;
  if( isDestroy ){
    sqlite3 *db = p->db;
    spellfix1DbExec(&rc, db, "DROP TABLE IF EXISTS \"%w\".\"%w_vocab\"",
                  p->zDbName, p->zTableName);
  }
  if( rc==SQLITE_OK ){
    sqlite3_free(p->zTableName);
    editDist3ConfigDelete(p->pConfig3);
    sqlite3_free(p->zCostTable);
    sqlite3_free(p);
  }
  return rc;
}
static int spellfix1Disconnect(sqlite3_vtab *pVTab){
  return spellfix1Uninit(0, pVTab);
}
static int spellfix1Destroy(sqlite3_vtab *pVTab){
  return spellfix1Uninit(1, pVTab);
}

/*
** Make a copy of a string.  Remove leading and trailing whitespace
** and dequote it.
*/
static char *spellfix1Dequote(const char *zIn){
  char *zOut;
  int i, j;
  char c;
  while( isspace((unsigned char)zIn[0]) ) zIn++;
  zOut = sqlite3_mprintf("%s", zIn);
  if( zOut==0 ) return 0;
  i = (int)strlen(zOut);
#if 0  /* The parser will never leave spaces at the end */
  while( i>0 && isspace(zOut[i-1]) ){ i--; }
#endif
  zOut[i] = 0;
  c = zOut[0];
  if( c=='\'' || c=='"' ){
    for(i=1, j=0; ALWAYS(zOut[i]); i++){
      zOut[j++] = zOut[i];
      if( zOut[i]==c ){
        if( zOut[i+1]==c ){
          i++;
        }else{
          zOut[j-1] = 0;
          break;
        }
      }
    }
  }
  return zOut;
}


/*
** xConnect/xCreate method for the spellfix1 module. Arguments are:
**
**   argv[0]   -> module name  ("spellfix1")
**   argv[1]   -> database name
**   argv[2]   -> table name
**   argv[3].. -> optional arguments (i.e. "edit_cost_table" parameter)
*/
static int spellfix1Init(
  int isCreate,
  sqlite3 *db,
  void *pAux,
  int argc, const char *const*argv,
  sqlite3_vtab **ppVTab,
  char **pzErr
){
  spellfix1_vtab *pNew = 0;
  /* const char *zModule = argv[0]; // not used */
  const char *zDbName = argv[1];
  const char *zTableName = argv[2];
  int nDbName;
  int rc = SQLITE_OK;
  int i;

  nDbName = (int)strlen(zDbName);
  pNew = sqlite3_malloc64( sizeof(*pNew) + nDbName + 1);
  if( pNew==0 ){
    rc = SQLITE_NOMEM;
  }else{
    memset(pNew, 0, sizeof(*pNew));
    pNew->zDbName = (char*)&pNew[1];
    memcpy(pNew->zDbName, zDbName, nDbName+1);
    pNew->zTableName = sqlite3_mprintf("%s", zTableName);
    pNew->db = db;
    if( pNew->zTableName==0 ){
      rc = SQLITE_NOMEM;
    }else{
      sqlite3_vtab_config(db, SQLITE_VTAB_INNOCUOUS);
      rc = sqlite3_declare_vtab(db, 
           "CREATE TABLE x(word,rank,distance,langid, "
           "score, matchlen, phonehash HIDDEN, "
           "top HIDDEN, scope HIDDEN, srchcnt HIDDEN, "
           "soundslike HIDDEN, command HIDDEN)"
      );
#define SPELLFIX_COL_WORD            0
#define SPELLFIX_COL_RANK            1
#define SPELLFIX_COL_DISTANCE        2
#define SPELLFIX_COL_LANGID          3
#define SPELLFIX_COL_SCORE           4
#define SPELLFIX_COL_MATCHLEN        5
#define SPELLFIX_COL_PHONEHASH       6
#define SPELLFIX_COL_TOP             7
#define SPELLFIX_COL_SCOPE           8
#define SPELLFIX_COL_SRCHCNT         9
#define SPELLFIX_COL_SOUNDSLIKE     10
#define SPELLFIX_COL_COMMAND        11
    }
    if( rc==SQLITE_OK && isCreate ){
      spellfix1DbExec(&rc, db,
         "CREATE TABLE IF NOT EXISTS \"%w\".\"%w_vocab\"(\n"
         "  id INTEGER PRIMARY KEY,\n"
         "  rank INT,\n"
         "  langid INT,\n"
         "  word TEXT,\n"
         "  k1 TEXT,\n"
         "  k2 TEXT\n"
         ");\n",
         zDbName, zTableName
      );
      spellfix1DbExec(&rc, db,
         "CREATE INDEX IF NOT EXISTS \"%w\".\"%w_vocab_index_langid_k2\" "
            "ON \"%w_vocab\"(langid,k2);",
         zDbName, zTableName, zTableName
      );
    }
    for(i=3; rc==SQLITE_OK && i<argc; i++){
      if( strncmp(argv[i],"edit_cost_table=",16)==0 && pNew->zCostTable==0 ){
        pNew->zCostTable = spellfix1Dequote(&argv[i][16]);
        if( pNew->zCostTable==0 ) rc = SQLITE_NOMEM;
        continue;
      }
      *pzErr = sqlite3_mprintf("bad argument to spellfix1(): \"%s\"", argv[i]);
      rc = SQLITE_ERROR; 
    }
  }

  if( rc && pNew ){
    *ppVTab = 0;
    spellfix1Uninit(0, &pNew->base);
  }else{
    *ppVTab = (sqlite3_vtab *)pNew;
  }
  return rc;
}

/*
** The xConnect and xCreate methods
*/
static int spellfix1Connect(
  sqlite3 *db,
  void *pAux,
  int argc, const char *const*argv,
  sqlite3_vtab **ppVTab,
  char **pzErr
){
  return spellfix1Init(0, db, pAux, argc, argv, ppVTab, pzErr);
}
static int spellfix1Create(
  sqlite3 *db,
  void *pAux,
  int argc, const char *const*argv,
  sqlite3_vtab **ppVTab,
  char **pzErr
){
  return spellfix1Init(1, db, pAux, argc, argv, ppVTab, pzErr);
}

/*
** Clear all of the content from a cursor.
*/
static void spellfix1ResetCursor(spellfix1_cursor *pCur){
  int i;
  for(i=0; i<pCur->nRow; i++){
    sqlite3_free(pCur->a[i].zWord);
  }
  pCur->nRow = 0;
  pCur->iRow = 0;
  pCur->nSearch = 0;
  if( pCur->pFullScan ){
    sqlite3_finalize(pCur->pFullScan);
    pCur->pFullScan = 0;
  }
}

/*
** Resize the cursor to hold up to N rows of content
*/
static void spellfix1ResizeCursor(spellfix1_cursor *pCur, int N){
  struct spellfix1_row *aNew;
  assert( N>=pCur->nRow );
  aNew = sqlite3_realloc64(pCur->a, sizeof(pCur->a[0])*N);
  if( aNew==0 && N>0 ){
    spellfix1ResetCursor(pCur);
    sqlite3_free(pCur->a);
    pCur->nAlloc = 0;
    pCur->a = 0;
  }else{
    pCur->nAlloc = N;
    pCur->a = aNew;
  }
}


/*
** Close a fuzzy-search cursor.
*/
static int spellfix1Close(sqlite3_vtab_cursor *cur){
  spellfix1_cursor *pCur = (spellfix1_cursor *)cur;
  spellfix1ResetCursor(pCur);
  spellfix1ResizeCursor(pCur, 0);
  sqlite3_free(pCur->zPattern);
  sqlite3_free(pCur);
  return SQLITE_OK;
}

#define SPELLFIX_IDXNUM_MATCH  0x01         /* word MATCH $str */
#define SPELLFIX_IDXNUM_LANGID 0x02         /* langid == $langid */
#define SPELLFIX_IDXNUM_TOP    0x04         /* top = $top */
#define SPELLFIX_IDXNUM_SCOPE  0x08         /* scope = $scope */
#define SPELLFIX_IDXNUM_DISTLT 0x10         /* distance < $distance */
#define SPELLFIX_IDXNUM_DISTLE 0x20         /* distance <= $distance */
#define SPELLFIX_IDXNUM_ROWID  0x40         /* rowid = $rowid */
#define SPELLFIX_IDXNUM_DIST   (0x10|0x20)  /* DISTLT and DISTLE */

/*
**
** The plan number is a bitmask of the SPELLFIX_IDXNUM_* values defined
** above.
**
** filter.argv[*] values contains $str, $langid, $top, $scope and $rowid
** if specified and in that order.
*/
static int spellfix1BestIndex(sqlite3_vtab *tab, sqlite3_index_info *pIdxInfo){
  int iPlan = 0;
  int iLangTerm = -1;
  int iTopTerm = -1;
  int iScopeTerm = -1;
  int iDistTerm = -1;
  int iRowidTerm = -1;
  int i;
  const struct sqlite3_index_constraint *pConstraint;
  pConstraint = pIdxInfo->aConstraint;
  for(i=0; i<pIdxInfo->nConstraint; i++, pConstraint++){
    if( pConstraint->usable==0 ) continue;

    /* Terms of the form:  word MATCH $str */
    if( (iPlan & SPELLFIX_IDXNUM_MATCH)==0 
     && pConstraint->iColumn==SPELLFIX_COL_WORD
     && pConstraint->op==SQLITE_INDEX_CONSTRAINT_MATCH
    ){
      iPlan |= SPELLFIX_IDXNUM_MATCH;
      pIdxInfo->aConstraintUsage[i].argvIndex = 1;
      pIdxInfo->aConstraintUsage[i].omit = 1;
    }

    /* Terms of the form:  langid = $langid  */
    if( (iPlan & SPELLFIX_IDXNUM_LANGID)==0
     && pConstraint->iColumn==SPELLFIX_COL_LANGID
     && pConstraint->op==SQLITE_INDEX_CONSTRAINT_EQ
    ){
      iPlan |= SPELLFIX_IDXNUM_LANGID;
      iLangTerm = i;
    }

    /* Terms of the form:  top = $top */
    if( (iPlan & SPELLFIX_IDXNUM_TOP)==0
     && pConstraint->iColumn==SPELLFIX_COL_TOP
     && pConstraint->op==SQLITE_INDEX_CONSTRAINT_EQ
    ){
      iPlan |= SPELLFIX_IDXNUM_TOP;
      iTopTerm = i;
    }

    /* Terms of the form:  scope = $scope */
    if( (iPlan & SPELLFIX_IDXNUM_SCOPE)==0
     && pConstraint->iColumn==SPELLFIX_COL_SCOPE
     && pConstraint->op==SQLITE_INDEX_CONSTRAINT_EQ
    ){
      iPlan |= SPELLFIX_IDXNUM_SCOPE;
      iScopeTerm = i;
    }

    /* Terms of the form:  distance < $dist or distance <= $dist */
    if( (iPlan & SPELLFIX_IDXNUM_DIST)==0
     && pConstraint->iColumn==SPELLFIX_COL_DISTANCE
     && (pConstraint->op==SQLITE_INDEX_CONSTRAINT_LT
          || pConstraint->op==SQLITE_INDEX_CONSTRAINT_LE)
    ){
      if( pConstraint->op==SQLITE_INDEX_CONSTRAINT_LT ){
        iPlan |= SPELLFIX_IDXNUM_DISTLT;
      }else{
        iPlan |= SPELLFIX_IDXNUM_DISTLE;
      }
      iDistTerm = i;
    }

    /* Terms of the form:  distance < $dist or distance <= $dist */
    if( (iPlan & SPELLFIX_IDXNUM_ROWID)==0
     && pConstraint->iColumn<0
     && pConstraint->op==SQLITE_INDEX_CONSTRAINT_EQ
    ){
      iPlan |= SPELLFIX_IDXNUM_ROWID;
      iRowidTerm = i;
    }
  }
  if( iPlan&SPELLFIX_IDXNUM_MATCH ){
    int idx = 2;
    pIdxInfo->idxNum = iPlan;
    if( pIdxInfo->nOrderBy==1
     && pIdxInfo->aOrderBy[0].iColumn==SPELLFIX_COL_SCORE
     && pIdxInfo->aOrderBy[0].desc==0
    ){
      pIdxInfo->orderByConsumed = 1;  /* Default order by iScore */
    }
    if( iPlan&SPELLFIX_IDXNUM_LANGID ){
      pIdxInfo->aConstraintUsage[iLangTerm].argvIndex = idx++;
      pIdxInfo->aConstraintUsage[iLangTerm].omit = 1;
    }
    if( iPlan&SPELLFIX_IDXNUM_TOP ){
      pIdxInfo->aConstraintUsage[iTopTerm].argvIndex = idx++;
      pIdxInfo->aConstraintUsage[iTopTerm].omit = 1;
    }
    if( iPlan&SPELLFIX_IDXNUM_SCOPE ){
      pIdxInfo->aConstraintUsage[iScopeTerm].argvIndex = idx++;
      pIdxInfo->aConstraintUsage[iScopeTerm].omit = 1;
    }
    if( iPlan&SPELLFIX_IDXNUM_DIST ){
      pIdxInfo->aConstraintUsage[iDistTerm].argvIndex = idx++;
      pIdxInfo->aConstraintUsage[iDistTerm].omit = 1;
    }
    pIdxInfo->estimatedCost = 1e5;
  }else if( (iPlan & SPELLFIX_IDXNUM_ROWID) ){
    pIdxInfo->idxNum = SPELLFIX_IDXNUM_ROWID;
    pIdxInfo->aConstraintUsage[iRowidTerm].argvIndex = 1;
    pIdxInfo->aConstraintUsage[iRowidTerm].omit = 1;
    pIdxInfo->estimatedCost = 5;
  }else{
    pIdxInfo->idxNum = 0;
    pIdxInfo->estimatedCost = 1e50;
  }
  return SQLITE_OK;
}

/*
** Open a new fuzzy-search cursor.
*/
static int spellfix1Open(sqlite3_vtab *pVTab, sqlite3_vtab_cursor **ppCursor){
  spellfix1_vtab *p = (spellfix1_vtab*)pVTab;
  spellfix1_cursor *pCur;
  pCur = sqlite3_malloc64( sizeof(*pCur) );
  if( pCur==0 ) return SQLITE_NOMEM;
  memset(pCur, 0, sizeof(*pCur));
  pCur->pVTab = p;
  *ppCursor = &pCur->base;
  return SQLITE_OK;
}

/*
** Adjust a distance measurement by the words rank in order to show
** preference to common words.
*/
static int spellfix1Score(int iDistance, int iRank){
  int iLog2;
  for(iLog2=0; iRank>0; iLog2++, iRank>>=1){}
  return iDistance + 32 - iLog2;
}

/*
** Compare two spellfix1_row objects for sorting purposes in qsort() such
** that they sort in order of increasing distance.
*/
static int SQLITE_CDECL spellfix1RowCompare(const void *A, const void *B){
  const struct spellfix1_row *a = (const struct spellfix1_row*)A;
  const struct spellfix1_row *b = (const struct spellfix1_row*)B;
  return a->iScore - b->iScore;
}

/*
** A structure used to pass information from spellfix1FilterForMatch()
** into spellfix1RunQuery().
*/
typedef struct MatchQuery {
  spellfix1_cursor *pCur;          /* The cursor being queried */
  sqlite3_stmt *pStmt;             /* shadow table query statment */
  char zHash[SPELLFIX_MX_HASH];    /* The current phonehash for zPattern */
  const char *zPattern;            /* Transliterated input string */
  int nPattern;                    /* Length of zPattern */
  EditDist3FromString *pMatchStr3; /* Original unicode string */
  EditDist3Config *pConfig3;       /* Edit-distance cost coefficients */
  const EditDist3Lang *pLang;      /* The selected language coefficients */
  int iLang;                       /* The language id */
  int iScope;                      /* Default scope */
  int iMaxDist;                    /* Maximum allowed edit distance, or -1 */
  int rc;                          /* Error code */
  int nRun;                  /* Number of prior runs for the same zPattern */
  char azPrior[SPELLFIX_MX_RUN][SPELLFIX_MX_HASH];  /* Prior hashes */
} MatchQuery;

/*
** Run a query looking for the best matches against zPattern using
** zHash as the character class seed hash.
*/
static void spellfix1RunQuery(MatchQuery *p, const char *zQuery, int nQuery){
  const char *zK1;
  const char *zWord;
  int iDist;
  int iRank;
  int iScore;
  int iWorst = 0;
  int idx;
  int idxWorst = -1;
  int i;
  int iScope = p->iScope;
  spellfix1_cursor *pCur = p->pCur;
  sqlite3_stmt *pStmt = p->pStmt;
  char zHash1[SPELLFIX_MX_HASH];
  char zHash2[SPELLFIX_MX_HASH];
  char *zClass;
  int nClass;
  int rc;

  if( pCur->a==0 || p->rc ) return;   /* Prior memory allocation failure */
  zClass = (char*)phoneticHash((unsigned char*)zQuery, nQuery);
  if( zClass==0 ){
    p->rc = SQLITE_NOMEM;
    return;
  }
  nClass = (int)strlen(zClass);
  if( nClass>SPELLFIX_MX_HASH-2 ){
    nClass = SPELLFIX_MX_HASH-2;
    zClass[nClass] = 0;
  }
  if( nClass<=iScope ){
    if( nClass>2 ){
      iScope = nClass-1;
    }else{
      iScope = nClass;
    }
  }
  memcpy(zHash1, zClass, iScope);
  sqlite3_free(zClass);
  zHash1[iScope] = 0;
  memcpy(zHash2, zHash1, iScope);
  zHash2[iScope] = 'Z';
  zHash2[iScope+1] = 0;
#if SPELLFIX_MX_RUN>1
  for(i=0; i<p->nRun; i++){
    if( strcmp(p->azPrior[i], zHash1)==0 ) return;
  }
#endif
  assert( p->nRun<SPELLFIX_MX_RUN );
  memcpy(p->azPrior[p->nRun++], zHash1, iScope+1);
  if( sqlite3_bind_text(pStmt, 1, zHash1, -1, SQLITE_STATIC)==SQLITE_NOMEM
   || sqlite3_bind_text(pStmt, 2, zHash2, -1, SQLITE_STATIC)==SQLITE_NOMEM
  ){
    p->rc = SQLITE_NOMEM;
    return;
  }
#if SPELLFIX_MX_RUN>1
  for(i=0; i<pCur->nRow; i++){
    if( pCur->a[i].iScore>iWorst ){
      iWorst = pCur->a[i].iScore;
      idxWorst = i;
    }
  }
#endif
  while( sqlite3_step(pStmt)==SQLITE_ROW ){
    int iMatchlen = -1;
    iRank = sqlite3_column_int(pStmt, 2);
    if( p->pMatchStr3 ){
      int nWord = sqlite3_column_bytes(pStmt, 1);
      zWord = (const char*)sqlite3_column_text(pStmt, 1);
      iDist = editDist3Core(p->pMatchStr3, zWord, nWord, p->pLang, &iMatchlen);
    }else{
      zK1 = (const char*)sqlite3_column_text(pStmt, 3);
      if( zK1==0 ) continue;
      iDist = editdist1(p->zPattern, zK1, 0);
    }
    if( iDist<0 ){
      p->rc = SQLITE_NOMEM;
      break;
    }
    pCur->nSearch++;
    
    /* If there is a "distance < $dist" or "distance <= $dist" constraint,
    ** check if this row meets it. If not, jump back up to the top of the
    ** loop to process the next row. Otherwise, if the row does match the
    ** distance constraint, check if the pCur->a[] array is already full.
    ** If it is and no explicit "top = ?" constraint was present in the
    ** query, grow the array to ensure there is room for the new entry. */
    assert( (p->iMaxDist>=0)==((pCur->idxNum & SPELLFIX_IDXNUM_DIST) ? 1 : 0) );
    if( p->iMaxDist>=0 ){
      if( iDist>p->iMaxDist ) continue;
      if( pCur->nRow>=pCur->nAlloc && (pCur->idxNum & SPELLFIX_IDXNUM_TOP)==0 ){
        spellfix1ResizeCursor(pCur, pCur->nAlloc*2 + 10);
        if( pCur->a==0 ) break;
      }
    }

    iScore = spellfix1Score(iDist,iRank);
    if( pCur->nRow<pCur->nAlloc ){
      idx = pCur->nRow;
    }else if( iScore<iWorst ){
      idx = idxWorst;
      sqlite3_free(pCur->a[idx].zWord);
    }else{
      continue;
    }

    pCur->a[idx].zWord = sqlite3_mprintf("%s", sqlite3_column_text(pStmt, 1));
    if( pCur->a[idx].zWord==0 ){
      p->rc = SQLITE_NOMEM;
      break;
    }
    pCur->a[idx].iRowid = sqlite3_column_int64(pStmt, 0);
    pCur->a[idx].iRank = iRank;
    pCur->a[idx].iDistance = iDist;
    pCur->a[idx].iScore = iScore;
    pCur->a[idx].iMatchlen = iMatchlen;
    memcpy(pCur->a[idx].zHash, zHash1, iScope+1);
    if( pCur->nRow<pCur->nAlloc ) pCur->nRow++;
    if( pCur->nRow==pCur->nAlloc ){
      iWorst = pCur->a[0].iScore;
      idxWorst = 0;
      for(i=1; i<pCur->nRow; i++){
        iScore = pCur->a[i].iScore;
        if( iWorst<iScore ){
          iWorst = iScore;
          idxWorst = i;
        }
      }
    }
  }
  rc = sqlite3_reset(pStmt);
  if( rc ) p->rc = rc;
}

/*
** This version of the xFilter method work if the MATCH term is present
** and we are doing a scan.
*/
static int spellfix1FilterForMatch(
  spellfix1_cursor *pCur,
  int argc,
  sqlite3_value **argv
){
  int idxNum = pCur->idxNum;
  const unsigned char *zMatchThis;   /* RHS of the MATCH operator */
  EditDist3FromString *pMatchStr3 = 0; /* zMatchThis as an editdist string */
  char *zPattern;                    /* Transliteration of zMatchThis */
  int nPattern;                      /* Length of zPattern */
  int iLimit = 20;                   /* Max number of rows of output */
  int iScope = 3;                    /* Use this many characters of zClass */
  int iLang = 0;                     /* Language code */
  char *zSql;                        /* SQL of shadow table query */
  sqlite3_stmt *pStmt = 0;           /* Shadow table query */
  int rc;                            /* Result code */
  int idx = 1;                       /* Next available filter parameter */
  spellfix1_vtab *p = pCur->pVTab;   /* The virtual table that owns pCur */
  MatchQuery x;                      /* For passing info to RunQuery() */

  /* Load the cost table if we have not already done so */
  if( p->zCostTable!=0 && p->pConfig3==0 ){
    p->pConfig3 = sqlite3_malloc64( sizeof(p->pConfig3[0]) );
    if( p->pConfig3==0 ) return SQLITE_NOMEM;
    memset(p->pConfig3, 0, sizeof(p->pConfig3[0]));
    rc = editDist3ConfigLoad(p->pConfig3, p->db, p->zCostTable);
    if( rc ) return rc;
  }
  memset(&x, 0, sizeof(x));
  x.iScope = 3;  /* Default scope if none specified by "WHERE scope=N" */
  x.iMaxDist = -1;   /* Maximum allowed edit distance */

  if( idxNum&2 ){
    iLang = sqlite3_value_int(argv[idx++]);
  }
  if( idxNum&4 ){
    iLimit = sqlite3_value_int(argv[idx++]);
    if( iLimit<1 ) iLimit = 1;
  }
  if( idxNum&8 ){
    x.iScope = sqlite3_value_int(argv[idx++]);
    if( x.iScope<1 ) x.iScope = 1;
    if( x.iScope>SPELLFIX_MX_HASH-2 ) x.iScope = SPELLFIX_MX_HASH-2;
  }
  if( idxNum&(16|32) ){
    x.iMaxDist = sqlite3_value_int(argv[idx++]);
    if( idxNum&16 ) x.iMaxDist--;
    if( x.iMaxDist<0 ) x.iMaxDist = 0;
  }
  spellfix1ResetCursor(pCur);
  spellfix1ResizeCursor(pCur, iLimit);
  zMatchThis = sqlite3_value_text(argv[0]);
  if( zMatchThis==0 ) return SQLITE_OK;
  if( p->pConfig3 ){
    x.pLang = editDist3FindLang(p->pConfig3, iLang);
    pMatchStr3 = editDist3FromStringNew(x.pLang, (const char*)zMatchThis, -1);
    if( pMatchStr3==0 ){
      x.rc = SQLITE_NOMEM;
      goto filter_exit;
    }
  }else{
    x.pLang = 0;
  }
  zPattern = (char*)transliterate(zMatchThis, sqlite3_value_bytes(argv[0]));
  sqlite3_free(pCur->zPattern);
  pCur->zPattern = zPattern;
  if( zPattern==0 ){
    x.rc = SQLITE_NOMEM;
    goto filter_exit;
  }
  nPattern = (int)strlen(zPattern);
  if( zPattern[nPattern-1]=='*' ) nPattern--;
  zSql = sqlite3_mprintf(
     "SELECT id, word, rank, coalesce(k1,word)"
     "  FROM \"%w\".\"%w_vocab\""
     " WHERE langid=%d AND k2>=?1 AND k2<?2",
     p->zDbName, p->zTableName, iLang
  );
  if( zSql==0 ){
    x.rc = SQLITE_NOMEM;
    pStmt = 0;
    goto filter_exit;
  }
  rc = sqlite3_prepare_v2(p->db, zSql, -1, &pStmt, 0);
  sqlite3_free(zSql);
  pCur->iLang = iLang;
  x.pCur = pCur;
  x.pStmt = pStmt;
  x.zPattern = zPattern;
  x.nPattern = nPattern;
  x.pMatchStr3 = pMatchStr3;
  x.iLang = iLang;
  x.rc = rc;
  x.pConfig3 = p->pConfig3;
  if( x.rc==SQLITE_OK ){
    spellfix1RunQuery(&x, zPattern, nPattern);
  }

  if( pCur->a ){
    qsort(pCur->a, pCur->nRow, sizeof(pCur->a[0]), spellfix1RowCompare);
    pCur->iTop = iLimit;
    pCur->iScope = iScope;
  }else{
    x.rc = SQLITE_NOMEM;
  }

filter_exit:
  sqlite3_finalize(pStmt);
  editDist3FromStringDelete(pMatchStr3);
  return x.rc;
}

/*
** This version of xFilter handles a full-table scan case
*/
static int spellfix1FilterForFullScan(
  spellfix1_cursor *pCur,
  int argc,
  sqlite3_value **argv
){
  int rc = SQLITE_OK;
  int idxNum = pCur->idxNum;
  char *zSql;
  spellfix1_vtab *pVTab = pCur->pVTab;
  spellfix1ResetCursor(pCur);
  assert( idxNum==0 || idxNum==64 );
  zSql = sqlite3_mprintf(
     "SELECT word, rank, NULL, langid, id FROM \"%w\".\"%w_vocab\"%s",
     pVTab->zDbName, pVTab->zTableName,
     ((idxNum & 64) ? " WHERE rowid=?" : "")
  );
  if( zSql==0 ) return SQLITE_NOMEM;
  rc = sqlite3_prepare_v2(pVTab->db, zSql, -1, &pCur->pFullScan, 0);
  sqlite3_free(zSql);
  if( rc==SQLITE_OK && (idxNum & 64) ){
    assert( argc==1 );
    rc = sqlite3_bind_value(pCur->pFullScan, 1, argv[0]);
  }
  pCur->nRow = pCur->iRow = 0;
  if( rc==SQLITE_OK ){
    rc = sqlite3_step(pCur->pFullScan);
    if( rc==SQLITE_ROW ){ pCur->iRow = -1; rc = SQLITE_OK; }
    if( rc==SQLITE_DONE ){ rc = SQLITE_OK; }
  }else{
    pCur->iRow = 0;
  }
  return rc;
}


/*
** Called to "rewind" a cursor back to the beginning so that
** it starts its output over again.  Always called at least once
** prior to any spellfix1Column, spellfix1Rowid, or spellfix1Eof call.
*/
static int spellfix1Filter(
  sqlite3_vtab_cursor *cur, 
  int idxNum, const char *idxStr,
  int argc, sqlite3_value **argv
){
  spellfix1_cursor *pCur = (spellfix1_cursor *)cur;
  int rc;
  pCur->idxNum = idxNum;
  if( idxNum & 1 ){
    rc = spellfix1FilterForMatch(pCur, argc, argv);
  }else{
    rc = spellfix1FilterForFullScan(pCur, argc, argv);
  }
  return rc;
}


/*
** Advance a cursor to its next row of output
*/
static int spellfix1Next(sqlite3_vtab_cursor *cur){
  spellfix1_cursor *pCur = (spellfix1_cursor *)cur;
  int rc = SQLITE_OK;
  if( pCur->iRow < pCur->nRow ){
    if( pCur->pFullScan ){
      rc = sqlite3_step(pCur->pFullScan);
      if( rc!=SQLITE_ROW ) pCur->iRow = pCur->nRow;
      if( rc==SQLITE_ROW || rc==SQLITE_DONE ) rc = SQLITE_OK;
    }else{
      pCur->iRow++;
    }
  }
  return rc;
}

/*
** Return TRUE if we are at the end-of-file
*/
static int spellfix1Eof(sqlite3_vtab_cursor *cur){
  spellfix1_cursor *pCur = (spellfix1_cursor *)cur;
  return pCur->iRow>=pCur->nRow;
}

/*
** Return columns from the current row.
*/
static int spellfix1Column(
  sqlite3_vtab_cursor *cur,
  sqlite3_context *ctx,
  int i
){
  spellfix1_cursor *pCur = (spellfix1_cursor*)cur;
  if( pCur->pFullScan ){
    if( i<=SPELLFIX_COL_LANGID ){
      sqlite3_result_value(ctx, sqlite3_column_value(pCur->pFullScan, i));
    }else{
      sqlite3_result_null(ctx);
    }
    return SQLITE_OK;
  }
  switch( i ){
    case SPELLFIX_COL_WORD: {
      sqlite3_result_text(ctx, pCur->a[pCur->iRow].zWord, -1, SQLITE_STATIC);
      break;
    }
    case SPELLFIX_COL_RANK: {
      sqlite3_result_int(ctx, pCur->a[pCur->iRow].iRank);
      break;
    }
    case SPELLFIX_COL_DISTANCE: {
      sqlite3_result_int(ctx, pCur->a[pCur->iRow].iDistance);
      break;
    }
    case SPELLFIX_COL_LANGID: {
      sqlite3_result_int(ctx, pCur->iLang);
      break;
    }
    case SPELLFIX_COL_SCORE: {
      sqlite3_result_int(ctx, pCur->a[pCur->iRow].iScore);
      break;
    }
    case SPELLFIX_COL_MATCHLEN: {
      int iMatchlen = pCur->a[pCur->iRow].iMatchlen;
      if( iMatchlen<0 ){
        int nPattern = (int)strlen(pCur->zPattern);
        char *zWord = pCur->a[pCur->iRow].zWord;
        int nWord = (int)strlen(zWord);

        if( nPattern>0 && pCur->zPattern[nPattern-1]=='*' ){
          char *zTranslit;
          int res;
          zTranslit = (char *)transliterate((unsigned char *)zWord, nWord);
          if( !zTranslit ) return SQLITE_NOMEM;
          res = editdist1(pCur->zPattern, zTranslit, &iMatchlen);
          sqlite3_free(zTranslit);
          if( res<0 ) return SQLITE_NOMEM;
          iMatchlen = translen_to_charlen(zWord, nWord, iMatchlen);
        }else{
          iMatchlen = utf8Charlen(zWord, nWord);
        }
      }

      sqlite3_result_int(ctx, iMatchlen);
      break;
    }
    case SPELLFIX_COL_PHONEHASH: {
      sqlite3_result_text(ctx, pCur->a[pCur->iRow].zHash, -1, SQLITE_STATIC);
      break;
    }
    case SPELLFIX_COL_TOP: {
      sqlite3_result_int(ctx, pCur->iTop);
      break;
    }
    case SPELLFIX_COL_SCOPE: {
      sqlite3_result_int(ctx, pCur->iScope);
      break;
    }
    case SPELLFIX_COL_SRCHCNT: {
      sqlite3_result_int(ctx, pCur->nSearch);
      break;
    }
    default: {
      sqlite3_result_null(ctx);
      break;
    }
  }
  return SQLITE_OK;
}

/*
** The rowid.
*/
static int spellfix1Rowid(sqlite3_vtab_cursor *cur, sqlite_int64 *pRowid){
  spellfix1_cursor *pCur = (spellfix1_cursor*)cur;
  if( pCur->pFullScan ){
    *pRowid = sqlite3_column_int64(pCur->pFullScan, 4);
  }else{
    *pRowid = pCur->a[pCur->iRow].iRowid;
  }
  return SQLITE_OK;
}

/*
** This function is called by the xUpdate() method. It returns a string
** containing the conflict mode that xUpdate() should use for the current
** operation. One of: "ROLLBACK", "IGNORE", "ABORT" or "REPLACE".
*/
static const char *spellfix1GetConflict(sqlite3 *db){
  static const char *azConflict[] = {
    /* Note: Instead of "FAIL" - "ABORT". */
    "ROLLBACK", "IGNORE", "ABORT", "ABORT", "REPLACE"
  };
  int eConflict = sqlite3_vtab_on_conflict(db);

  assert( eConflict==SQLITE_ROLLBACK || eConflict==SQLITE_IGNORE
       || eConflict==SQLITE_FAIL || eConflict==SQLITE_ABORT
       || eConflict==SQLITE_REPLACE
  );
  assert( SQLITE_ROLLBACK==1 );
  assert( SQLITE_IGNORE==2 );
  assert( SQLITE_FAIL==3 );
  assert( SQLITE_ABORT==4 );
  assert( SQLITE_REPLACE==5 );

  return azConflict[eConflict-1];
}

/*
** The xUpdate() method.
*/
static int spellfix1Update(
  sqlite3_vtab *pVTab,
  int argc,
  sqlite3_value **argv,
  sqlite_int64 *pRowid
){
  int rc = SQLITE_OK;
  sqlite3_int64 rowid, newRowid;
  spellfix1_vtab *p = (spellfix1_vtab*)pVTab;
  sqlite3 *db = p->db;

  if( argc==1 ){
    /* A delete operation on the rowid given by argv[0] */
    rowid = *pRowid = sqlite3_value_int64(argv[0]);
    spellfix1DbExec(&rc, db, "DELETE FROM \"%w\".\"%w_vocab\" "
                           " WHERE id=%lld",
                  p->zDbName, p->zTableName, rowid);
  }else{
    const unsigned char *zWord = sqlite3_value_text(argv[SPELLFIX_COL_WORD+2]);
    int nWord = sqlite3_value_bytes(argv[SPELLFIX_COL_WORD+2]);
    int iLang = sqlite3_value_int(argv[SPELLFIX_COL_LANGID+2]);
    int iRank = sqlite3_value_int(argv[SPELLFIX_COL_RANK+2]);
    const unsigned char *zSoundslike =
           sqlite3_value_text(argv[SPELLFIX_COL_SOUNDSLIKE+2]);
    int nSoundslike = sqlite3_value_bytes(argv[SPELLFIX_COL_SOUNDSLIKE+2]);
    char *zK1, *zK2;
    int i;
    char c;
    const char *zConflict = spellfix1GetConflict(db);

    if( zWord==0 ){
      /* Inserts of the form:  INSERT INTO table(command) VALUES('xyzzy');
      ** cause zWord to be NULL, so we look at the "command" column to see
      ** what special actions to take */
      const char *zCmd = 
         (const char*)sqlite3_value_text(argv[SPELLFIX_COL_COMMAND+2]);
      if( zCmd==0 ){
        pVTab->zErrMsg = sqlite3_mprintf("NOT NULL constraint failed: %s.word",
                                         p->zTableName);
        return SQLITE_CONSTRAINT_NOTNULL;
      }
      if( strcmp(zCmd,"reset")==0 ){
        /* Reset the  edit cost table (if there is one). */
        editDist3ConfigDelete(p->pConfig3);
        p->pConfig3 = 0;
        return SQLITE_OK;
      }
      if( strncmp(zCmd,"edit_cost_table=",16)==0 ){
        editDist3ConfigDelete(p->pConfig3);
        p->pConfig3 = 0;
        sqlite3_free(p->zCostTable);
        p->zCostTable = spellfix1Dequote(zCmd+16);
        if( p->zCostTable==0 ) return SQLITE_NOMEM;
        if( p->zCostTable[0]==0 || sqlite3_stricmp(p->zCostTable,"null")==0 ){
          sqlite3_free(p->zCostTable);
          p->zCostTable = 0;
        }
        return SQLITE_OK;
      }
      pVTab->zErrMsg = sqlite3_mprintf("unknown value for %s.command: \"%w\"",
                                       p->zTableName, zCmd);
      return SQLITE_ERROR;
    }
    if( iRank<1 ) iRank = 1;
    if( zSoundslike ){
      zK1 = (char*)transliterate(zSoundslike, nSoundslike);
    }else{
      zK1 = (char*)transliterate(zWord, nWord);
    }
    if( zK1==0 ) return SQLITE_NOMEM;
    for(i=0; (c = zK1[i])!=0; i++){
       if( c>='A' && c<='Z' ) zK1[i] += 'a' - 'A';
    }
    zK2 = (char*)phoneticHash((const unsigned char*)zK1, i);
    if( zK2==0 ){
      sqlite3_free(zK1);
      return SQLITE_NOMEM;
    }
    if( sqlite3_value_type(argv[0])==SQLITE_NULL ){
      if( sqlite3_value_type(argv[1])==SQLITE_NULL ){
        spellfix1DbExec(&rc, db,
               "INSERT INTO \"%w\".\"%w_vocab\"(rank,langid,word,k1,k2) "
               "VALUES(%d,%d,%Q,nullif(%Q,%Q),%Q)",
               p->zDbName, p->zTableName,
               iRank, iLang, zWord, zK1, zWord, zK2
        );
      }else{
        newRowid = sqlite3_value_int64(argv[1]);
        spellfix1DbExec(&rc, db,
            "INSERT OR %s INTO \"%w\".\"%w_vocab\"(id,rank,langid,word,k1,k2) "
            "VALUES(%lld,%d,%d,%Q,nullif(%Q,%Q),%Q)",
            zConflict, p->zDbName, p->zTableName,
            newRowid, iRank, iLang, zWord, zK1, zWord, zK2
        );
      }
      *pRowid = sqlite3_last_insert_rowid(db);
    }else{
      rowid = sqlite3_value_int64(argv[0]);
      newRowid = *pRowid = sqlite3_value_int64(argv[1]);
      spellfix1DbExec(&rc, db,
             "UPDATE OR %s \"%w\".\"%w_vocab\" SET id=%lld, rank=%d, langid=%d,"
             " word=%Q, k1=nullif(%Q,%Q), k2=%Q WHERE id=%lld",
             zConflict, p->zDbName, p->zTableName, newRowid, iRank, iLang,
             zWord, zK1, zWord, zK2, rowid
      );
    }
    sqlite3_free(zK1);
    sqlite3_free(zK2);
  }
  return rc;
}

/*
** Rename the spellfix1 table.
*/
static int spellfix1Rename(sqlite3_vtab *pVTab, const char *zNew){
  spellfix1_vtab *p = (spellfix1_vtab*)pVTab;
  sqlite3 *db = p->db;
  int rc = SQLITE_OK;
  char *zNewName = sqlite3_mprintf("%s", zNew);
  if( zNewName==0 ){
    return SQLITE_NOMEM;
  }
  spellfix1DbExec(&rc, db, 
     "ALTER TABLE \"%w\".\"%w_vocab\" RENAME TO \"%w_vocab\"",
     p->zDbName, p->zTableName, zNewName
  );
  if( rc==SQLITE_OK ){
    sqlite3_free(p->zTableName);
    p->zTableName = zNewName;
  }else{
    sqlite3_free(zNewName);
  }
  return rc;
}


/*
** A virtual table module that provides fuzzy search.
*/
static sqlite3_module spellfix1Module = {
  0,                       /* iVersion */
  spellfix1Create,         /* xCreate - handle CREATE VIRTUAL TABLE */
  spellfix1Connect,        /* xConnect - reconnected to an existing table */
  spellfix1BestIndex,      /* xBestIndex - figure out how to do a query */
  spellfix1Disconnect,     /* xDisconnect - close a connection */
  spellfix1Destroy,        /* xDestroy - handle DROP TABLE */
  spellfix1Open,           /* xOpen - open a cursor */
  spellfix1Close,          /* xClose - close a cursor */
  spellfix1Filter,         /* xFilter - configure scan constraints */
  spellfix1Next,           /* xNext - advance a cursor */
  spellfix1Eof,            /* xEof - check for end of scan */
  spellfix1Column,         /* xColumn - read data */
  spellfix1Rowid,          /* xRowid - read data */
  spellfix1Update,         /* xUpdate */
  0,                       /* xBegin */
  0,                       /* xSync */
  0,                       /* xCommit */
  0,                       /* xRollback */
  0,                       /* xFindMethod */
  spellfix1Rename,         /* xRename */
};

/*
** Register the various functions and the virtual table.
*/
static int spellfix1Register(sqlite3 *db){
  int rc = SQLITE_OK;
  int i;
  rc = sqlite3_create_function(db, "spellfix1_translit", 1,
                               SQLITE_UTF8|SQLITE_DETERMINISTIC, 0,
                                transliterateSqlFunc, 0, 0);
  if( rc==SQLITE_OK ){
    rc = sqlite3_create_function(db, "spellfix1_editdist", 2,
                                 SQLITE_UTF8|SQLITE_DETERMINISTIC, 0,
                                  editdistSqlFunc, 0, 0);
  }
  if( rc==SQLITE_OK ){
    rc = sqlite3_create_function(db, "spellfix1_phonehash", 1,
                                 SQLITE_UTF8|SQLITE_DETERMINISTIC, 0,
                                  phoneticHashSqlFunc, 0, 0);
  }
  if( rc==SQLITE_OK ){
    rc = sqlite3_create_function(db, "spellfix1_scriptcode", 1,
                                  SQLITE_UTF8|SQLITE_DETERMINISTIC, 0,
                                  scriptCodeSqlFunc, 0, 0);
  }
  if( rc==SQLITE_OK ){
    rc = sqlite3_create_module(db, "spellfix1", &spellfix1Module, 0);
  }
  if( rc==SQLITE_OK ){
    rc = editDist3Install(db);
  }

  /* Verify sanity of the translit[] table */
  for(i=0; i<sizeof(translit)/sizeof(translit[0])-1; i++){
    assert( translit[i].cFrom<translit[i+1].cFrom );
  }

  return rc;
}

#endif /* SQLITE_OMIT_VIRTUALTABLE */

/*
** Extension load function.
*/
#ifdef _WIN32
__declspec(dllexport)
#endif
int sqlite3_spellfix_init(
  sqlite3 *db, 
  char **pzErrMsg, 
  const sqlite3_api_routines *pApi
){
  SQLITE_EXTENSION_INIT2(pApi);
#ifndef SQLITE_OMIT_VIRTUALTABLE
  return spellfix1Register(db);
#endif
  return SQLITE_OK;
}

/*
** This is an extension initializer that is a no-op and always
** succeeds, except that it fails if the fault-simulation is set
** to 500.
*/
static int sqlite3TestExtInit(sqlite3 *db){
  (void)db;
  return sqlite3FaultSim(500);
}


/*
** Forward declarations of external module initializer functions
** for modules that need them.
*/
#ifdef SQLITE_ENABLE_FTS1
int sqlite3Fts1Init(sqlite3*);
#endif
#ifdef SQLITE_ENABLE_FTS2
int sqlite3Fts2Init(sqlite3*);
#endif
#ifdef SQLITE_ENABLE_FTS5
int sqlite3Fts5Init(sqlite3*);
#endif
#ifdef SQLITE_ENABLE_STMTVTAB
int sqlite3StmtVtabInit(sqlite3*);
#endif

/*
** An array of pointers to extension initializer functions for
** built-in extensions.
*/
static int (*const sqlite3BuiltinExtensions[])(sqlite3*) = {
#ifdef SQLITE_ENABLE_FTS1
  sqlite3Fts1Init,
#endif
#ifdef SQLITE_ENABLE_FTS2
  sqlite3Fts2Init,
#endif
#ifdef SQLITE_ENABLE_FTS3
  sqlite3Fts3Init,
#endif
#ifdef SQLITE_ENABLE_FTS5
  sqlite3Fts5Init,
#endif
#if defined(SQLITE_ENABLE_ICU) || defined(SQLITE_ENABLE_ICU_COLLATIONS)
  sqlite3IcuInit,
#endif
#ifdef SQLITE_ENABLE_RTREE
  sqlite3RtreeInit,
#endif
#ifdef SQLITE_ENABLE_DBPAGE_VTAB
  sqlite3DbpageRegister,
#endif
#ifdef SQLITE_ENABLE_DBSTAT_VTAB
  sqlite3DbstatRegister,
#endif
  sqlite3TestExtInit,
#if !defined(SQLITE_OMIT_VIRTUALTABLE) && !defined(SQLITE_OMIT_JSON)
  sqlite3JsonTableFunctions,
#endif
#ifdef SQLITE_ENABLE_STMTVTAB
  sqlite3StmtVtabInit,
#endif
#ifdef SQLITE_ENABLE_BYTECODE_VTAB
  sqlite3VdbeBytecodeVtabInit,
#endif
};

#ifndef SQLITE_AMALGAMATION
/* IMPLEMENTATION-OF: R-46656-45156 The sqlite3_version[] string constant
** contains the text of SQLITE_VERSION macro.
*/
const char sqlite3_version[] = SQLITE_VERSION;
#endif

/* IMPLEMENTATION-OF: R-53536-42575 The sqlite3_libversion() function returns
** a pointer to the to the sqlite3_version[] string constant. 
*/
const char *sqlite3_libversion(void){ return sqlite3_version; }

/* IMPLEMENTATION-OF: R-25063-23286 The sqlite3_sourceid() function returns a
** pointer to a string constant whose value is the same as the
** SQLITE_SOURCE_ID C preprocessor macro. Except if SQLite is built using
** an edited copy of the amalgamation, then the last four characters of
** the hash might be different from SQLITE_SOURCE_ID.
*/
const char *sqlite3_sourceid(void){ return SQLITE_SOURCE_ID; }

/* IMPLEMENTATION-OF: R-35210-63508 The sqlite3_libversion_number() function
** returns an integer equal to SQLITE_VERSION_NUMBER.
*/
int sqlite3_libversion_number(void){ return SQLITE_VERSION_NUMBER; }

/* IMPLEMENTATION-OF: R-20790-14025 The sqlite3_threadsafe() function returns
** zero if and only if SQLite was compiled with mutexing code omitted due to
** the SQLITE_THREADSAFE compile-time option being set to 0.
*/
int sqlite3_threadsafe(void){ return SQLITE_THREADSAFE; }

/*
** When compiling the test fixture or with debugging enabled (on Win32),
** this variable being set to non-zero will cause OSTRACE macros to emit
** extra diagnostic information.
*/
#ifdef SQLITE_HAVE_OS_TRACE
# ifndef SQLITE_DEBUG_OS_TRACE
#   define SQLITE_DEBUG_OS_TRACE 0
# endif
  int sqlite3OSTrace = SQLITE_DEBUG_OS_TRACE;
#endif

#if !defined(SQLITE_OMIT_TRACE) && defined(SQLITE_ENABLE_IOTRACE)
/*
** If the following function pointer is not NULL and if
** SQLITE_ENABLE_IOTRACE is enabled, then messages describing
** I/O active are written using this function.  These messages
** are intended for debugging activity only.
*/
SQLITE_API void (SQLITE_CDECL *sqlite3IoTrace)(const char*, ...) = 0;
#endif

/*
** If the following global variable points to a string which is the
** name of a directory, then that directory will be used to store
** temporary files.
**
** See also the "PRAGMA temp_store_directory" SQL command.
*/
char *sqlite3_temp_directory = 0;

/*
** If the following global variable points to a string which is the
** name of a directory, then that directory will be used to store
** all database files specified with a relative pathname.
**
** See also the "PRAGMA data_store_directory" SQL command.
*/
char *sqlite3_data_directory = 0;

/*
** Initialize SQLite.  
**
** This routine must be called to initialize the memory allocation,
** VFS, and mutex subsystems prior to doing any serious work with
** SQLite.  But as long as you do not compile with SQLITE_OMIT_AUTOINIT
** this routine will be called automatically by key routines such as
** sqlite3_open().  
**
** This routine is a no-op except on its very first call for the process,
** or for the first call after a call to sqlite3_shutdown.
**
** The first thread to call this routine runs the initialization to
** completion.  If subsequent threads call this routine before the first
** thread has finished the initialization process, then the subsequent
** threads must block until the first thread finishes with the initialization.
**
** The first thread might call this routine recursively.  Recursive
** calls to this routine should not block, of course.  Otherwise the
** initialization process would never complete.
**
** Let X be the first thread to enter this routine.  Let Y be some other
** thread.  Then while the initial invocation of this routine by X is
** incomplete, it is required that:
**
**    *  Calls to this routine from Y must block until the outer-most
**       call by X completes.
**
**    *  Recursive calls to this routine from thread X return immediately
**       without blocking.
*/
int sqlite3_initialize(void){
  MUTEX_LOGIC( sqlite3_mutex *pMainMtx; )      /* The main static mutex */
  int rc;                                      /* Result code */
#ifdef SQLITE_EXTRA_INIT
  int bRunExtraInit = 0;                       /* Extra initialization needed */
#endif

#ifdef SQLITE_OMIT_WSD
  rc = sqlite3_wsd_init(4096, 24);
  if( rc!=SQLITE_OK ){
    return rc;
  }
#endif

  /* If the following assert() fails on some obscure processor/compiler
  ** combination, the work-around is to set the correct pointer
  ** size at compile-time using -DSQLITE_PTRSIZE=n compile-time option */
  assert( SQLITE_PTRSIZE==sizeof(char*) );

  /* If SQLite is already completely initialized, then this call
  ** to sqlite3_initialize() should be a no-op.  But the initialization
  ** must be complete.  So isInit must not be set until the very end
  ** of this routine.
  */
  if( sqlite3GlobalConfig.isInit ){
    sqlite3MemoryBarrier();
    return SQLITE_OK;
  }

  /* Make sure the mutex subsystem is initialized.  If unable to 
  ** initialize the mutex subsystem, return early with the error.
  ** If the system is so sick that we are unable to allocate a mutex,
  ** there is not much SQLite is going to be able to do.
  **
  ** The mutex subsystem must take care of serializing its own
  ** initialization.
  */
  rc = sqlite3MutexInit();
  if( rc ) return rc;

  /* Initialize the malloc() system and the recursive pInitMutex mutex.
  ** This operation is protected by the STATIC_MAIN mutex.  Note that
  ** MutexAlloc() is called for a static mutex prior to initializing the
  ** malloc subsystem - this implies that the allocation of a static
  ** mutex must not require support from the malloc subsystem.
  */
  MUTEX_LOGIC( pMainMtx = sqlite3MutexAlloc(SQLITE_MUTEX_STATIC_MAIN); )
  sqlite3_mutex_enter(pMainMtx);
  sqlite3GlobalConfig.isMutexInit = 1;
  if( !sqlite3GlobalConfig.isMallocInit ){
    rc = sqlite3MallocInit();
  }
  if( rc==SQLITE_OK ){
    sqlite3GlobalConfig.isMallocInit = 1;
    if( !sqlite3GlobalConfig.pInitMutex ){
      sqlite3GlobalConfig.pInitMutex =
           sqlite3MutexAlloc(SQLITE_MUTEX_RECURSIVE);
      if( sqlite3GlobalConfig.bCoreMutex && !sqlite3GlobalConfig.pInitMutex ){
        rc = SQLITE_NOMEM_BKPT;
      }
    }
  }
  if( rc==SQLITE_OK ){
    sqlite3GlobalConfig.nRefInitMutex++;
  }
  sqlite3_mutex_leave(pMainMtx);

  /* If rc is not SQLITE_OK at this point, then either the malloc
  ** subsystem could not be initialized or the system failed to allocate
  ** the pInitMutex mutex. Return an error in either case.  */
  if( rc!=SQLITE_OK ){
    return rc;
  }

  /* Do the rest of the initialization under the recursive mutex so
  ** that we will be able to handle recursive calls into
  ** sqlite3_initialize().  The recursive calls normally come through
  ** sqlite3_os_init() when it invokes sqlite3_vfs_register(), but other
  ** recursive calls might also be possible.
  **
  ** IMPLEMENTATION-OF: R-00140-37445 SQLite automatically serializes calls
  ** to the xInit method, so the xInit method need not be threadsafe.
  **
  ** The following mutex is what serializes access to the appdef pcache xInit
  ** methods.  The sqlite3_pcache_methods.xInit() all is embedded in the
  ** call to sqlite3PcacheInitialize().
  */
  sqlite3_mutex_enter(sqlite3GlobalConfig.pInitMutex);
  if( sqlite3GlobalConfig.isInit==0 && sqlite3GlobalConfig.inProgress==0 ){
    sqlite3GlobalConfig.inProgress = 1;
#ifdef SQLITE_ENABLE_SQLLOG
    {
      extern void sqlite3_init_sqllog(void);
      sqlite3_init_sqllog();
    }
#endif
    memset(&sqlite3BuiltinFunctions, 0, sizeof(sqlite3BuiltinFunctions));
    sqlite3RegisterBuiltinFunctions();
    if( sqlite3GlobalConfig.isPCacheInit==0 ){
      rc = sqlite3PcacheInitialize();
    }
    if( rc==SQLITE_OK ){
      sqlite3GlobalConfig.isPCacheInit = 1;
      rc = sqlite3OsInit();
    }
#ifndef SQLITE_OMIT_DESERIALIZE
    if( rc==SQLITE_OK ){
      rc = sqlite3MemdbInit();
    }
#endif
    if( rc==SQLITE_OK ){
      sqlite3PCacheBufferSetup( sqlite3GlobalConfig.pPage, 
          sqlite3GlobalConfig.szPage, sqlite3GlobalConfig.nPage);
      sqlite3MemoryBarrier();
      sqlite3GlobalConfig.isInit = 1;
#ifdef SQLITE_EXTRA_INIT
      bRunExtraInit = 1;
#endif
    }
    sqlite3GlobalConfig.inProgress = 0;
  }
  sqlite3_mutex_leave(sqlite3GlobalConfig.pInitMutex);

  /* Go back under the static mutex and clean up the recursive
  ** mutex to prevent a resource leak.
  */
  sqlite3_mutex_enter(pMainMtx);
  sqlite3GlobalConfig.nRefInitMutex--;
  if( sqlite3GlobalConfig.nRefInitMutex<=0 ){
    assert( sqlite3GlobalConfig.nRefInitMutex==0 );
    sqlite3_mutex_free(sqlite3GlobalConfig.pInitMutex);
    sqlite3GlobalConfig.pInitMutex = 0;
  }
  sqlite3_mutex_leave(pMainMtx);

  /* The following is just a sanity check to make sure SQLite has
  ** been compiled correctly.  It is important to run this code, but
  ** we don't want to run it too often and soak up CPU cycles for no
  ** reason.  So we run it once during initialization.
  */
#ifndef NDEBUG
#ifndef SQLITE_OMIT_FLOATING_POINT
  /* This section of code's only "output" is via assert() statements. */
  if( rc==SQLITE_OK ){
    u64 x = (((u64)1)<<63)-1;
    double y;
    assert(sizeof(x)==8);
    assert(sizeof(x)==sizeof(y));
    memcpy(&y, &x, 8);
    assert( sqlite3IsNaN(y) );
  }
#endif
#endif

  /* Do extra initialization steps requested by the SQLITE_EXTRA_INIT
  ** compile-time option.
  */
#ifdef SQLITE_EXTRA_INIT
  if( bRunExtraInit ){
    int SQLITE_EXTRA_INIT(const char*);
    rc = SQLITE_EXTRA_INIT(0);
  }
#endif

  return rc;
}

/*
** Undo the effects of sqlite3_initialize().  Must not be called while
** there are outstanding database connections or memory allocations or
** while any part of SQLite is otherwise in use in any thread.  This
** routine is not threadsafe.  But it is safe to invoke this routine
** on when SQLite is already shut down.  If SQLite is already shut down
** when this routine is invoked, then this routine is a harmless no-op.
*/
int sqlite3_shutdown(void){
#ifdef SQLITE_OMIT_WSD
  int rc = sqlite3_wsd_init(4096, 24);
  if( rc!=SQLITE_OK ){
    return rc;
  }
#endif

  if( sqlite3GlobalConfig.isInit ){
#ifdef SQLITE_EXTRA_SHUTDOWN
    void SQLITE_EXTRA_SHUTDOWN(void);
    SQLITE_EXTRA_SHUTDOWN();
#endif
    sqlite3_os_end();
    sqlite3_reset_auto_extension();
    sqlite3GlobalConfig.isInit = 0;
  }
  if( sqlite3GlobalConfig.isPCacheInit ){
    sqlite3PcacheShutdown();
    sqlite3GlobalConfig.isPCacheInit = 0;
  }
  if( sqlite3GlobalConfig.isMallocInit ){
    sqlite3MallocEnd();
    sqlite3GlobalConfig.isMallocInit = 0;

#ifndef SQLITE_OMIT_SHUTDOWN_DIRECTORIES
    /* The heap subsystem has now been shutdown and these values are supposed
    ** to be NULL or point to memory that was obtained from sqlite3_malloc(),
    ** which would rely on that heap subsystem; therefore, make sure these
    ** values cannot refer to heap memory that was just invalidated when the
    ** heap subsystem was shutdown.  This is only done if the current call to
    ** this function resulted in the heap subsystem actually being shutdown.
    */
    sqlite3_data_directory = 0;
    sqlite3_temp_directory = 0;
#endif
  }
  if( sqlite3GlobalConfig.isMutexInit ){
    sqlite3MutexEnd();
    sqlite3GlobalConfig.isMutexInit = 0;
  }

  return SQLITE_OK;
}

/*
** This API allows applications to modify the global configuration of
** the SQLite library at run-time.
**
** This routine should only be called when there are no outstanding
** database connections or memory allocations.  This routine is not
** threadsafe.  Failure to heed these warnings can lead to unpredictable
** behavior.
*/
int sqlite3_config(int op, ...){
  va_list ap;
  int rc = SQLITE_OK;

  /* sqlite3_config() normally returns SQLITE_MISUSE if it is invoked while
  ** the SQLite library is in use.  Except, a few selected opcodes
  ** are allowed.
  */
  if( sqlite3GlobalConfig.isInit ){
    static const u64 mAnytimeConfigOption = 0
       | MASKBIT64( SQLITE_CONFIG_LOG )
       | MASKBIT64( SQLITE_CONFIG_PCACHE_HDRSZ )
    ;
    if( op<0 || op>63 || (MASKBIT64(op) & mAnytimeConfigOption)==0 ){
      return SQLITE_MISUSE_BKPT;
    }
    testcase( op==SQLITE_CONFIG_LOG );
    testcase( op==SQLITE_CONFIG_PCACHE_HDRSZ );
  }

  va_start(ap, op);
  switch( op ){

    /* Mutex configuration options are only available in a threadsafe
    ** compile.
    */
#if defined(SQLITE_THREADSAFE) && SQLITE_THREADSAFE>0  /* IMP: R-54466-46756 */
    case SQLITE_CONFIG_SINGLETHREAD: {
      /* EVIDENCE-OF: R-02748-19096 This option sets the threading mode to
      ** Single-thread. */
      sqlite3GlobalConfig.bCoreMutex = 0;  /* Disable mutex on core */
      sqlite3GlobalConfig.bFullMutex = 0;  /* Disable mutex on connections */
      break;
    }
#endif
#if defined(SQLITE_THREADSAFE) && SQLITE_THREADSAFE>0 /* IMP: R-20520-54086 */
    case SQLITE_CONFIG_MULTITHREAD: {
      /* EVIDENCE-OF: R-14374-42468 This option sets the threading mode to
      ** Multi-thread. */
      sqlite3GlobalConfig.bCoreMutex = 1;  /* Enable mutex on core */
      sqlite3GlobalConfig.bFullMutex = 0;  /* Disable mutex on connections */
      break;
    }
#endif
#if defined(SQLITE_THREADSAFE) && SQLITE_THREADSAFE>0 /* IMP: R-59593-21810 */
    case SQLITE_CONFIG_SERIALIZED: {
      /* EVIDENCE-OF: R-41220-51800 This option sets the threading mode to
      ** Serialized. */
      sqlite3GlobalConfig.bCoreMutex = 1;  /* Enable mutex on core */
      sqlite3GlobalConfig.bFullMutex = 1;  /* Enable mutex on connections */
      break;
    }
#endif
#if defined(SQLITE_THREADSAFE) && SQLITE_THREADSAFE>0 /* IMP: R-63666-48755 */
    case SQLITE_CONFIG_MUTEX: {
      /* Specify an alternative mutex implementation */
      sqlite3GlobalConfig.mutex = *va_arg(ap, sqlite3_mutex_methods*);
      break;
    }
#endif
#if defined(SQLITE_THREADSAFE) && SQLITE_THREADSAFE>0 /* IMP: R-14450-37597 */
    case SQLITE_CONFIG_GETMUTEX: {
      /* Retrieve the current mutex implementation */
      *va_arg(ap, sqlite3_mutex_methods*) = sqlite3GlobalConfig.mutex;
      break;
    }
#endif

    case SQLITE_CONFIG_MALLOC: {
      /* EVIDENCE-OF: R-55594-21030 The SQLITE_CONFIG_MALLOC option takes a
      ** single argument which is a pointer to an instance of the
      ** sqlite3_mem_methods structure. The argument specifies alternative
      ** low-level memory allocation routines to be used in place of the memory
      ** allocation routines built into SQLite. */
      sqlite3GlobalConfig.m = *va_arg(ap, sqlite3_mem_methods*);
      break;
    }
    case SQLITE_CONFIG_GETMALLOC: {
      /* EVIDENCE-OF: R-51213-46414 The SQLITE_CONFIG_GETMALLOC option takes a
      ** single argument which is a pointer to an instance of the
      ** sqlite3_mem_methods structure. The sqlite3_mem_methods structure is
      ** filled with the currently defined memory allocation routines. */
      if( sqlite3GlobalConfig.m.xMalloc==0 ) sqlite3MemSetDefault();
      *va_arg(ap, sqlite3_mem_methods*) = sqlite3GlobalConfig.m;
      break;
    }
    case SQLITE_CONFIG_MEMSTATUS: {
      assert( !sqlite3GlobalConfig.isInit );  /* Cannot change at runtime */
      /* EVIDENCE-OF: R-61275-35157 The SQLITE_CONFIG_MEMSTATUS option takes
      ** single argument of type int, interpreted as a boolean, which enables
      ** or disables the collection of memory allocation statistics. */
      sqlite3GlobalConfig.bMemstat = va_arg(ap, int);
      break;
    }
    case SQLITE_CONFIG_SMALL_MALLOC: {
      sqlite3GlobalConfig.bSmallMalloc = va_arg(ap, int);
      break;
    }
    case SQLITE_CONFIG_PAGECACHE: {
      /* EVIDENCE-OF: R-18761-36601 There are three arguments to
      ** SQLITE_CONFIG_PAGECACHE: A pointer to 8-byte aligned memory (pMem),
      ** the size of each page cache line (sz), and the number of cache lines
      ** (N). */
      sqlite3GlobalConfig.pPage = va_arg(ap, void*);
      sqlite3GlobalConfig.szPage = va_arg(ap, int);
      sqlite3GlobalConfig.nPage = va_arg(ap, int);
      break;
    }
    case SQLITE_CONFIG_PCACHE_HDRSZ: {
      /* EVIDENCE-OF: R-39100-27317 The SQLITE_CONFIG_PCACHE_HDRSZ option takes
      ** a single parameter which is a pointer to an integer and writes into
      ** that integer the number of extra bytes per page required for each page
      ** in SQLITE_CONFIG_PAGECACHE. */
      *va_arg(ap, int*) = 
          sqlite3HeaderSizeBtree() +
          sqlite3HeaderSizePcache() +
          sqlite3HeaderSizePcache1();
      break;
    }

    case SQLITE_CONFIG_PCACHE: {
      /* no-op */
      break;
    }
    case SQLITE_CONFIG_GETPCACHE: {
      /* now an error */
      rc = SQLITE_ERROR;
      break;
    }

    case SQLITE_CONFIG_PCACHE2: {
      /* EVIDENCE-OF: R-63325-48378 The SQLITE_CONFIG_PCACHE2 option takes a
      ** single argument which is a pointer to an sqlite3_pcache_methods2
      ** object. This object specifies the interface to a custom page cache
      ** implementation. */
      sqlite3GlobalConfig.pcache2 = *va_arg(ap, sqlite3_pcache_methods2*);
      break;
    }
    case SQLITE_CONFIG_GETPCACHE2: {
      /* EVIDENCE-OF: R-22035-46182 The SQLITE_CONFIG_GETPCACHE2 option takes a
      ** single argument which is a pointer to an sqlite3_pcache_methods2
      ** object. SQLite copies of the current page cache implementation into
      ** that object. */
      if( sqlite3GlobalConfig.pcache2.xInit==0 ){
        sqlite3PCacheSetDefault();
      }
      *va_arg(ap, sqlite3_pcache_methods2*) = sqlite3GlobalConfig.pcache2;
      break;
    }

/* EVIDENCE-OF: R-06626-12911 The SQLITE_CONFIG_HEAP option is only
** available if SQLite is compiled with either SQLITE_ENABLE_MEMSYS3 or
** SQLITE_ENABLE_MEMSYS5 and returns SQLITE_ERROR if invoked otherwise. */
#if defined(SQLITE_ENABLE_MEMSYS3) || defined(SQLITE_ENABLE_MEMSYS5)
    case SQLITE_CONFIG_HEAP: {
      /* EVIDENCE-OF: R-19854-42126 There are three arguments to
      ** SQLITE_CONFIG_HEAP: An 8-byte aligned pointer to the memory, the
      ** number of bytes in the memory buffer, and the minimum allocation size.
      */
      sqlite3GlobalConfig.pHeap = va_arg(ap, void*);
      sqlite3GlobalConfig.nHeap = va_arg(ap, int);
      sqlite3GlobalConfig.mnReq = va_arg(ap, int);

      if( sqlite3GlobalConfig.mnReq<1 ){
        sqlite3GlobalConfig.mnReq = 1;
      }else if( sqlite3GlobalConfig.mnReq>(1<<12) ){
        /* cap min request size at 2^12 */
        sqlite3GlobalConfig.mnReq = (1<<12);
      }

      if( sqlite3GlobalConfig.pHeap==0 ){
        /* EVIDENCE-OF: R-49920-60189 If the first pointer (the memory pointer)
        ** is NULL, then SQLite reverts to using its default memory allocator
        ** (the system malloc() implementation), undoing any prior invocation of
        ** SQLITE_CONFIG_MALLOC.
        **
        ** Setting sqlite3GlobalConfig.m to all zeros will cause malloc to
        ** revert to its default implementation when sqlite3_initialize() is run
        */
        memset(&sqlite3GlobalConfig.m, 0, sizeof(sqlite3GlobalConfig.m));
      }else{
        /* EVIDENCE-OF: R-61006-08918 If the memory pointer is not NULL then the
        ** alternative memory allocator is engaged to handle all of SQLites
        ** memory allocation needs. */
#ifdef SQLITE_ENABLE_MEMSYS3
        sqlite3GlobalConfig.m = *sqlite3MemGetMemsys3();
#endif
#ifdef SQLITE_ENABLE_MEMSYS5
        sqlite3GlobalConfig.m = *sqlite3MemGetMemsys5();
#endif
      }
      break;
    }
#endif

    case SQLITE_CONFIG_LOOKASIDE: {
      sqlite3GlobalConfig.szLookaside = va_arg(ap, int);
      sqlite3GlobalConfig.nLookaside = va_arg(ap, int);
      break;
    }
    
    /* Record a pointer to the logger function and its first argument.
    ** The default is NULL.  Logging is disabled if the function pointer is
    ** NULL.
    */
    case SQLITE_CONFIG_LOG: {
      /* MSVC is picky about pulling func ptrs from va lists.
      ** http://support.microsoft.com/kb/47961
      ** sqlite3GlobalConfig.xLog = va_arg(ap, void(*)(void*,int,const char*));
      */
      typedef void(*LOGFUNC_t)(void*,int,const char*);
      LOGFUNC_t xLog = va_arg(ap, LOGFUNC_t);
      void *pLogArg = va_arg(ap, void*);
      AtomicStore(&sqlite3GlobalConfig.xLog, xLog);
      AtomicStore(&sqlite3GlobalConfig.pLogArg, pLogArg);
      break;
    }

    /* EVIDENCE-OF: R-55548-33817 The compile-time setting for URI filenames
    ** can be changed at start-time using the
    ** sqlite3_config(SQLITE_CONFIG_URI,1) or
    ** sqlite3_config(SQLITE_CONFIG_URI,0) configuration calls.
    */
    case SQLITE_CONFIG_URI: {
      /* EVIDENCE-OF: R-25451-61125 The SQLITE_CONFIG_URI option takes a single
      ** argument of type int. If non-zero, then URI handling is globally
      ** enabled. If the parameter is zero, then URI handling is globally
      ** disabled. */
      int bOpenUri = va_arg(ap, int);
      AtomicStore(&sqlite3GlobalConfig.bOpenUri, bOpenUri);
      break;
    }

    case SQLITE_CONFIG_COVERING_INDEX_SCAN: {
      /* EVIDENCE-OF: R-36592-02772 The SQLITE_CONFIG_COVERING_INDEX_SCAN
      ** option takes a single integer argument which is interpreted as a
      ** boolean in order to enable or disable the use of covering indices for
      ** full table scans in the query optimizer. */
      sqlite3GlobalConfig.bUseCis = va_arg(ap, int);
      break;
    }

#ifdef SQLITE_ENABLE_SQLLOG
    case SQLITE_CONFIG_SQLLOG: {
      typedef void(*SQLLOGFUNC_t)(void*, sqlite3*, const char*, int);
      sqlite3GlobalConfig.xSqllog = va_arg(ap, SQLLOGFUNC_t);
      sqlite3GlobalConfig.pSqllogArg = va_arg(ap, void *);
      break;
    }
#endif

    case SQLITE_CONFIG_MMAP_SIZE: {
      /* EVIDENCE-OF: R-58063-38258 SQLITE_CONFIG_MMAP_SIZE takes two 64-bit
      ** integer (sqlite3_int64) values that are the default mmap size limit
      ** (the default setting for PRAGMA mmap_size) and the maximum allowed
      ** mmap size limit. */
      sqlite3_int64 szMmap = va_arg(ap, sqlite3_int64);
      sqlite3_int64 mxMmap = va_arg(ap, sqlite3_int64);
      /* EVIDENCE-OF: R-53367-43190 If either argument to this option is
      ** negative, then that argument is changed to its compile-time default.
      **
      ** EVIDENCE-OF: R-34993-45031 The maximum allowed mmap size will be
      ** silently truncated if necessary so that it does not exceed the
      ** compile-time maximum mmap size set by the SQLITE_MAX_MMAP_SIZE
      ** compile-time option.
      */
      if( mxMmap<0 || mxMmap>SQLITE_MAX_MMAP_SIZE ){
        mxMmap = SQLITE_MAX_MMAP_SIZE;
      }
      if( szMmap<0 ) szMmap = SQLITE_DEFAULT_MMAP_SIZE;
      if( szMmap>mxMmap) szMmap = mxMmap;
      sqlite3GlobalConfig.mxMmap = mxMmap;
      sqlite3GlobalConfig.szMmap = szMmap;
      break;
    }

#if SQLITE_OS_WIN && defined(SQLITE_WIN32_MALLOC) /* IMP: R-04780-55815 */
    case SQLITE_CONFIG_WIN32_HEAPSIZE: {
      /* EVIDENCE-OF: R-34926-03360 SQLITE_CONFIG_WIN32_HEAPSIZE takes a 32-bit
      ** unsigned integer value that specifies the maximum size of the created
      ** heap. */
      sqlite3GlobalConfig.nHeap = va_arg(ap, int);
      break;
    }
#endif

    case SQLITE_CONFIG_PMASZ: {
      sqlite3GlobalConfig.szPma = va_arg(ap, unsigned int);
      break;
    }

    case SQLITE_CONFIG_STMTJRNL_SPILL: {
      sqlite3GlobalConfig.nStmtSpill = va_arg(ap, int);
      break;
    }

#ifdef SQLITE_ENABLE_SORTER_REFERENCES
    case SQLITE_CONFIG_SORTERREF_SIZE: {
      int iVal = va_arg(ap, int);
      if( iVal<0 ){
        iVal = SQLITE_DEFAULT_SORTERREF_SIZE;
      }
      sqlite3GlobalConfig.szSorterRef = (u32)iVal;
      break;
    }
#endif /* SQLITE_ENABLE_SORTER_REFERENCES */

#ifndef SQLITE_OMIT_DESERIALIZE
    case SQLITE_CONFIG_MEMDB_MAXSIZE: {
      sqlite3GlobalConfig.mxMemdbSize = va_arg(ap, sqlite3_int64);
      break;
    }
#endif /* SQLITE_OMIT_DESERIALIZE */

    default: {
      rc = SQLITE_ERROR;
      break;
    }
  }
  va_end(ap);
  return rc;
}

/*
** Set up the lookaside buffers for a database connection.
** Return SQLITE_OK on success.  
** If lookaside is already active, return SQLITE_BUSY.
**
** The sz parameter is the number of bytes in each lookaside slot.
** The cnt parameter is the number of slots.  If pStart is NULL the
** space for the lookaside memory is obtained from sqlite3_malloc().
** If pStart is not NULL then it is sz*cnt bytes of memory to use for
** the lookaside memory.
*/
static int setupLookaside(sqlite3 *db, void *pBuf, int sz, int cnt){
#ifndef SQLITE_OMIT_LOOKASIDE
  void *pStart;
  sqlite3_int64 szAlloc = sz*(sqlite3_int64)cnt;
  int nBig;   /* Number of full-size slots */
  int nSm;    /* Number smaller LOOKASIDE_SMALL-byte slots */
  
  if( sqlite3LookasideUsed(db,0)>0 ){
    return SQLITE_BUSY;
  }
  /* Free any existing lookaside buffer for this handle before
  ** allocating a new one so we don't have to have space for 
  ** both at the same time.
  */
  if( db->lookaside.bMalloced ){
    sqlite3_free(db->lookaside.pStart);
  }
  /* The size of a lookaside slot after ROUNDDOWN8 needs to be larger
  ** than a pointer to be useful.
  */
  sz = ROUNDDOWN8(sz);  /* IMP: R-33038-09382 */
  if( sz<=(int)sizeof(LookasideSlot*) ) sz = 0;
  if( cnt<0 ) cnt = 0;
  if( sz==0 || cnt==0 ){
    sz = 0;
    pStart = 0;
  }else if( pBuf==0 ){
    sqlite3BeginBenignMalloc();
    pStart = sqlite3Malloc( szAlloc );  /* IMP: R-61949-35727 */
    sqlite3EndBenignMalloc();
    if( pStart ) szAlloc = sqlite3MallocSize(pStart);
  }else{
    pStart = pBuf;
  }
#ifndef SQLITE_OMIT_TWOSIZE_LOOKASIDE
  if( sz>=LOOKASIDE_SMALL*3 ){
    nBig = szAlloc/(3*LOOKASIDE_SMALL+sz);
    nSm = (szAlloc - sz*nBig)/LOOKASIDE_SMALL;
  }else if( sz>=LOOKASIDE_SMALL*2 ){
    nBig = szAlloc/(LOOKASIDE_SMALL+sz);
    nSm = (szAlloc - sz*nBig)/LOOKASIDE_SMALL;
  }else
#endif /* SQLITE_OMIT_TWOSIZE_LOOKASIDE */
  if( sz>0 ){
    nBig = szAlloc/sz;
    nSm = 0;
  }else{
    nBig = nSm = 0;
  }
  db->lookaside.pStart = pStart;
  db->lookaside.pInit = 0;
  db->lookaside.pFree = 0;
  db->lookaside.sz = (u16)sz;
  db->lookaside.szTrue = (u16)sz;
  if( pStart ){
    int i;
    LookasideSlot *p;
    assert( sz > (int)sizeof(LookasideSlot*) );
    p = (LookasideSlot*)pStart;
    for(i=0; i<nBig; i++){
      p->pNext = db->lookaside.pInit;
      db->lookaside.pInit = p;
      p = (LookasideSlot*)&((u8*)p)[sz];
    }
#ifndef SQLITE_OMIT_TWOSIZE_LOOKASIDE
    db->lookaside.pSmallInit = 0;
    db->lookaside.pSmallFree = 0;
    db->lookaside.pMiddle = p;
    for(i=0; i<nSm; i++){
      p->pNext = db->lookaside.pSmallInit;
      db->lookaside.pSmallInit = p;
      p = (LookasideSlot*)&((u8*)p)[LOOKASIDE_SMALL];
    }
#endif /* SQLITE_OMIT_TWOSIZE_LOOKASIDE */
    assert( ((uptr)p)<=szAlloc + (uptr)pStart );
    db->lookaside.pEnd = p;
    db->lookaside.bDisable = 0;
    db->lookaside.bMalloced = pBuf==0 ?1:0;
    db->lookaside.nSlot = nBig+nSm;
  }else{
    db->lookaside.pStart = 0;
#ifndef SQLITE_OMIT_TWOSIZE_LOOKASIDE
    db->lookaside.pSmallInit = 0;
    db->lookaside.pSmallFree = 0;
    db->lookaside.pMiddle = 0;
#endif /* SQLITE_OMIT_TWOSIZE_LOOKASIDE */
    db->lookaside.pEnd = 0;
    db->lookaside.bDisable = 1;
    db->lookaside.sz = 0;
    db->lookaside.bMalloced = 0;
    db->lookaside.nSlot = 0;
  }
  db->lookaside.pTrueEnd = db->lookaside.pEnd;
  assert( sqlite3LookasideUsed(db,0)==0 );
#endif /* SQLITE_OMIT_LOOKASIDE */
  return SQLITE_OK;
}

/*
** Return the mutex associated with a database connection.
*/
sqlite3_mutex *sqlite3_db_mutex(sqlite3 *db){
#ifdef SQLITE_ENABLE_API_ARMOR
  if( !sqlite3SafetyCheckOk(db) ){
    (void)SQLITE_MISUSE_BKPT;
    return 0;
  }
#endif
  return db->mutex;
}

/*
** Free up as much memory as we can from the given database
** connection.
*/
int sqlite3_db_release_memory(sqlite3 *db){
  int i;

#ifdef SQLITE_ENABLE_API_ARMOR
  if( !sqlite3SafetyCheckOk(db) ) return SQLITE_MISUSE_BKPT;
#endif
  sqlite3_mutex_enter(db->mutex);
  sqlite3BtreeEnterAll(db);
  for(i=0; i<db->nDb; i++){
    Btree *pBt = db->aDb[i].pBt;
    if( pBt ){
      Pager *pPager = sqlite3BtreePager(pBt);
      sqlite3PagerShrink(pPager);
    }
  }
  sqlite3BtreeLeaveAll(db);
  sqlite3_mutex_leave(db->mutex);
  return SQLITE_OK;
}

/*
** Flush any dirty pages in the pager-cache for any attached database
** to disk.
*/
int sqlite3_db_cacheflush(sqlite3 *db){
  int i;
  int rc = SQLITE_OK;
  int bSeenBusy = 0;

#ifdef SQLITE_ENABLE_API_ARMOR
  if( !sqlite3SafetyCheckOk(db) ) return SQLITE_MISUSE_BKPT;
#endif
  sqlite3_mutex_enter(db->mutex);
  sqlite3BtreeEnterAll(db);
  for(i=0; rc==SQLITE_OK && i<db->nDb; i++){
    Btree *pBt = db->aDb[i].pBt;
    if( pBt && sqlite3BtreeTxnState(pBt)==SQLITE_TXN_WRITE ){
      Pager *pPager = sqlite3BtreePager(pBt);
      rc = sqlite3PagerFlush(pPager);
      if( rc==SQLITE_BUSY ){
        bSeenBusy = 1;
        rc = SQLITE_OK;
      }
    }
  }
  sqlite3BtreeLeaveAll(db);
  sqlite3_mutex_leave(db->mutex);
  return ((rc==SQLITE_OK && bSeenBusy) ? SQLITE_BUSY : rc);
}

/*
** Configuration settings for an individual database connection
*/
int sqlite3_db_config(sqlite3 *db, int op, ...){
  va_list ap;
  int rc;
  sqlite3_mutex_enter(db->mutex);
  va_start(ap, op);
  switch( op ){
    case SQLITE_DBCONFIG_MAINDBNAME: {
      /* IMP: R-06824-28531 */
      /* IMP: R-36257-52125 */
      db->aDb[0].zDbSName = va_arg(ap,char*);
      rc = SQLITE_OK;
      break;
    }
    case SQLITE_DBCONFIG_LOOKASIDE: {
      void *pBuf = va_arg(ap, void*); /* IMP: R-26835-10964 */
      int sz = va_arg(ap, int);       /* IMP: R-47871-25994 */
      int cnt = va_arg(ap, int);      /* IMP: R-04460-53386 */
      rc = setupLookaside(db, pBuf, sz, cnt);
      break;
    }
    default: {
      static const struct {
        int op;      /* The opcode */
        u32 mask;    /* Mask of the bit in sqlite3.flags to set/clear */
      } aFlagOp[] = {
        { SQLITE_DBCONFIG_ENABLE_FKEY,           SQLITE_ForeignKeys    },
        { SQLITE_DBCONFIG_ENABLE_TRIGGER,        SQLITE_EnableTrigger  },
        { SQLITE_DBCONFIG_ENABLE_VIEW,           SQLITE_EnableView     },
        { SQLITE_DBCONFIG_ENABLE_FTS3_TOKENIZER, SQLITE_Fts3Tokenizer  },
        { SQLITE_DBCONFIG_ENABLE_LOAD_EXTENSION, SQLITE_LoadExtension  },
        { SQLITE_DBCONFIG_NO_CKPT_ON_CLOSE,      SQLITE_NoCkptOnClose  },
        { SQLITE_DBCONFIG_ENABLE_QPSG,           SQLITE_EnableQPSG     },
        { SQLITE_DBCONFIG_TRIGGER_EQP,           SQLITE_TriggerEQP     },
        { SQLITE_DBCONFIG_RESET_DATABASE,        SQLITE_ResetDatabase  },
        { SQLITE_DBCONFIG_DEFENSIVE,             SQLITE_Defensive      },
        { SQLITE_DBCONFIG_WRITABLE_SCHEMA,       SQLITE_WriteSchema|
                                                 SQLITE_NoSchemaError  },
        { SQLITE_DBCONFIG_LEGACY_ALTER_TABLE,    SQLITE_LegacyAlter    },
        { SQLITE_DBCONFIG_DQS_DDL,               SQLITE_DqsDDL         },
        { SQLITE_DBCONFIG_DQS_DML,               SQLITE_DqsDML         },
        { SQLITE_DBCONFIG_LEGACY_FILE_FORMAT,    SQLITE_LegacyFileFmt  },
        { SQLITE_DBCONFIG_TRUSTED_SCHEMA,        SQLITE_TrustedSchema  },
        { SQLITE_DBCONFIG_STMT_SCANSTATUS,       SQLITE_StmtScanStatus },
        { SQLITE_DBCONFIG_REVERSE_SCANORDER,     SQLITE_ReverseOrder   },
      };
      unsigned int i;
      rc = SQLITE_ERROR; /* IMP: R-42790-23372 */
      for(i=0; i<ArraySize(aFlagOp); i++){
        if( aFlagOp[i].op==op ){
          int onoff = va_arg(ap, int);
          int *pRes = va_arg(ap, int*);
          u64 oldFlags = db->flags;
          if( onoff>0 ){
            db->flags |= aFlagOp[i].mask;
          }else if( onoff==0 ){
            db->flags &= ~(u64)aFlagOp[i].mask;
          }
          if( oldFlags!=db->flags ){
            sqlite3ExpirePreparedStatements(db, 0);
          }
          if( pRes ){
            *pRes = (db->flags & aFlagOp[i].mask)!=0;
          }
          rc = SQLITE_OK;
          break;
        }
      }
      break;
    }
  }
  va_end(ap);
  sqlite3_mutex_leave(db->mutex);
  return rc;
}

/*
** This is the default collating function named "BINARY" which is always
** available.
*/
static int binCollFunc(
  void *NotUsed,
  int nKey1, const void *pKey1,
  int nKey2, const void *pKey2
){
  int rc, n;
  UNUSED_PARAMETER(NotUsed);
  n = nKey1<nKey2 ? nKey1 : nKey2;
  /* EVIDENCE-OF: R-65033-28449 The built-in BINARY collation compares
  ** strings byte by byte using the memcmp() function from the standard C
  ** library. */
  assert( pKey1 && pKey2 );
  rc = memcmp(pKey1, pKey2, n);
  if( rc==0 ){
    rc = nKey1 - nKey2;
  }
  return rc;
}

/*
** This is the collating function named "RTRIM" which is always
** available.  Ignore trailing spaces.
*/
static int rtrimCollFunc(
  void *pUser,
  int nKey1, const void *pKey1,
  int nKey2, const void *pKey2
){
  const u8 *pK1 = (const u8*)pKey1;
  const u8 *pK2 = (const u8*)pKey2;
  while( nKey1 && pK1[nKey1-1]==' ' ) nKey1--;
  while( nKey2 && pK2[nKey2-1]==' ' ) nKey2--;
  return binCollFunc(pUser, nKey1, pKey1, nKey2, pKey2);
}

/*
** Return true if CollSeq is the default built-in BINARY.
*/
int sqlite3IsBinary(const CollSeq *p){
  assert( p==0 || p->xCmp!=binCollFunc || strcmp(p->zName,"BINARY")==0 );
  return p==0 || p->xCmp==binCollFunc;
}

/*
** Another built-in collating sequence: NOCASE. 
**
** This collating sequence is intended to be used for "case independent
** comparison". SQLite's knowledge of upper and lower case equivalents
** extends only to the 26 characters used in the English language.
**
** At the moment there is only a UTF-8 implementation.
*/
static int nocaseCollatingFunc(
  void *NotUsed,
  int nKey1, const void *pKey1,
  int nKey2, const void *pKey2
){
  int r = sqlite3StrNICmp(
      (const char *)pKey1, (const char *)pKey2, (nKey1<nKey2)?nKey1:nKey2);
  UNUSED_PARAMETER(NotUsed);
  if( 0==r ){
    r = nKey1-nKey2;
  }
  return r;
}

/*
** Return the ROWID of the most recent insert
*/
sqlite_int64 sqlite3_last_insert_rowid(sqlite3 *db){
#ifdef SQLITE_ENABLE_API_ARMOR
  if( !sqlite3SafetyCheckOk(db) ){
    (void)SQLITE_MISUSE_BKPT;
    return 0;
  }
#endif
  return db->lastRowid;
}

/*
** Set the value returned by the sqlite3_last_insert_rowid() API function.
*/
void sqlite3_set_last_insert_rowid(sqlite3 *db, sqlite3_int64 iRowid){
#ifdef SQLITE_ENABLE_API_ARMOR
  if( !sqlite3SafetyCheckOk(db) ){
    (void)SQLITE_MISUSE_BKPT;
    return;
  }
#endif
  sqlite3_mutex_enter(db->mutex);
  db->lastRowid = iRowid;
  sqlite3_mutex_leave(db->mutex);
}

/*
** Return the number of changes in the most recent call to sqlite3_exec().
*/
sqlite3_int64 sqlite3_changes64(sqlite3 *db){
#ifdef SQLITE_ENABLE_API_ARMOR
  if( !sqlite3SafetyCheckOk(db) ){
    (void)SQLITE_MISUSE_BKPT;
    return 0;
  }
#endif
  return db->nChange;
}
int sqlite3_changes(sqlite3 *db){
  return (int)sqlite3_changes64(db);
}

/*
** Return the number of changes since the database handle was opened.
*/
sqlite3_int64 sqlite3_total_changes64(sqlite3 *db){
#ifdef SQLITE_ENABLE_API_ARMOR
  if( !sqlite3SafetyCheckOk(db) ){
    (void)SQLITE_MISUSE_BKPT;
    return 0;
  }
#endif
  return db->nTotalChange;
}
int sqlite3_total_changes(sqlite3 *db){
  return (int)sqlite3_total_changes64(db);
}

/*
** Close all open savepoints. This function only manipulates fields of the
** database handle object, it does not close any savepoints that may be open
** at the b-tree/pager level.
*/
void sqlite3CloseSavepoints(sqlite3 *db){
  while( db->pSavepoint ){
    Savepoint *pTmp = db->pSavepoint;
    db->pSavepoint = pTmp->pNext;
    sqlite3DbFree(db, pTmp);
  }
  db->nSavepoint = 0;
  db->nStatement = 0;
  db->isTransactionSavepoint = 0;
}

/*
** Invoke the destructor function associated with FuncDef p, if any. Except,
** if this is not the last copy of the function, do not invoke it. Multiple
** copies of a single function are created when create_function() is called
** with SQLITE_ANY as the encoding.
*/
static void functionDestroy(sqlite3 *db, FuncDef *p){
  FuncDestructor *pDestructor;
  assert( (p->funcFlags & SQLITE_FUNC_BUILTIN)==0 );
  pDestructor = p->u.pDestructor;
  if( pDestructor ){
    pDestructor->nRef--;
    if( pDestructor->nRef==0 ){
      pDestructor->xDestroy(pDestructor->pUserData);
      sqlite3DbFree(db, pDestructor);
    }
  }
}

/*
** Disconnect all sqlite3_vtab objects that belong to database connection
** db. This is called when db is being closed.
*/
static void disconnectAllVtab(sqlite3 *db){
#ifndef SQLITE_OMIT_VIRTUALTABLE
  int i;
  HashElem *p;
  sqlite3BtreeEnterAll(db);
  for(i=0; i<db->nDb; i++){
    Schema *pSchema = db->aDb[i].pSchema;
    if( pSchema ){
      for(p=sqliteHashFirst(&pSchema->tblHash); p; p=sqliteHashNext(p)){
        Table *pTab = (Table *)sqliteHashData(p);
        if( IsVirtual(pTab) ) sqlite3VtabDisconnect(db, pTab);
      }
    }
  }
  for(p=sqliteHashFirst(&db->aModule); p; p=sqliteHashNext(p)){
    Module *pMod = (Module *)sqliteHashData(p);
    if( pMod->pEpoTab ){
      sqlite3VtabDisconnect(db, pMod->pEpoTab);
    }
  }
  sqlite3VtabUnlockList(db);
  sqlite3BtreeLeaveAll(db);
#else
  UNUSED_PARAMETER(db);
#endif
}

/*
** Return TRUE if database connection db has unfinalized prepared
** statements or unfinished sqlite3_backup objects.  
*/
static int connectionIsBusy(sqlite3 *db){
  int j;
  assert( sqlite3_mutex_held(db->mutex) );
  if( db->pVdbe ) return 1;
  for(j=0; j<db->nDb; j++){
    Btree *pBt = db->aDb[j].pBt;
    if( pBt && sqlite3BtreeIsInBackup(pBt) ) return 1;
  }
  return 0;
}

/*
** Close an existing SQLite database
*/
static int sqlite3Close(sqlite3 *db, int forceZombie){
  if( !db ){
    /* EVIDENCE-OF: R-63257-11740 Calling sqlite3_close() or
    ** sqlite3_close_v2() with a NULL pointer argument is a harmless no-op. */
    return SQLITE_OK;
  }
  if( !sqlite3SafetyCheckSickOrOk(db) ){
    return SQLITE_MISUSE_BKPT;
  }
  sqlite3_mutex_enter(db->mutex);
  if( db->mTrace & SQLITE_TRACE_CLOSE ){
    db->trace.xV2(SQLITE_TRACE_CLOSE, db->pTraceArg, db, 0);
  }

  /* Force xDisconnect calls on all virtual tables */
  disconnectAllVtab(db);

  /* If a transaction is open, the disconnectAllVtab() call above
  ** will not have called the xDisconnect() method on any virtual
  ** tables in the db->aVTrans[] array. The following sqlite3VtabRollback()
  ** call will do so. We need to do this before the check for active
  ** SQL statements below, as the v-table implementation may be storing
  ** some prepared statements internally.
  */
  sqlite3VtabRollback(db);

  /* Legacy behavior (sqlite3_close() behavior) is to return
  ** SQLITE_BUSY if the connection can not be closed immediately.
  */
  if( !forceZombie && connectionIsBusy(db) ){
    sqlite3ErrorWithMsg(db, SQLITE_BUSY, "unable to close due to unfinalized "
       "statements or unfinished backups");
    sqlite3_mutex_leave(db->mutex);
    return SQLITE_BUSY;
  }

#ifdef SQLITE_ENABLE_SQLLOG
  if( sqlite3GlobalConfig.xSqllog ){
    /* Closing the handle. Fourth parameter is passed the value 2. */
    sqlite3GlobalConfig.xSqllog(sqlite3GlobalConfig.pSqllogArg, db, 0, 2);
  }
#endif

  /* Convert the connection into a zombie and then close it.
  */
  db->eOpenState = SQLITE_STATE_ZOMBIE;
  sqlite3LeaveMutexAndCloseZombie(db);
  return SQLITE_OK;
}

/*
** Return the transaction state for a single databse, or the maximum
** transaction state over all attached databases if zSchema is null.
*/
int sqlite3_txn_state(sqlite3 *db, const char *zSchema){
  int iDb, nDb;
  int iTxn = -1;
#ifdef SQLITE_ENABLE_API_ARMOR
  if( !sqlite3SafetyCheckOk(db) ){
    (void)SQLITE_MISUSE_BKPT;
    return -1;
  }
#endif
  sqlite3_mutex_enter(db->mutex);
  if( zSchema ){
    nDb = iDb = sqlite3FindDbName(db, zSchema);
    if( iDb<0 ) nDb--;
  }else{
    iDb = 0;
    nDb = db->nDb-1;
  }
  for(; iDb<=nDb; iDb++){
    Btree *pBt = db->aDb[iDb].pBt;
    int x = pBt!=0 ? sqlite3BtreeTxnState(pBt) : SQLITE_TXN_NONE;
    if( x>iTxn ) iTxn = x;
  }
  sqlite3_mutex_leave(db->mutex);
  return iTxn;
}

/*
** Two variations on the public interface for closing a database
** connection. The sqlite3_close() version returns SQLITE_BUSY and
** leaves the connection open if there are unfinalized prepared
** statements or unfinished sqlite3_backups.  The sqlite3_close_v2()
** version forces the connection to become a zombie if there are
** unclosed resources, and arranges for deallocation when the last
** prepare statement or sqlite3_backup closes.
*/
int sqlite3_close(sqlite3 *db){ return sqlite3Close(db,0); }
int sqlite3_close_v2(sqlite3 *db){ return sqlite3Close(db,1); }


/*
** Close the mutex on database connection db.
**
** Furthermore, if database connection db is a zombie (meaning that there
** has been a prior call to sqlite3_close(db) or sqlite3_close_v2(db)) and
** every sqlite3_stmt has now been finalized and every sqlite3_backup has
** finished, then free all resources.
*/
void sqlite3LeaveMutexAndCloseZombie(sqlite3 *db){
  HashElem *i;                    /* Hash table iterator */
  int j;

  /* If there are outstanding sqlite3_stmt or sqlite3_backup objects
  ** or if the connection has not yet been closed by sqlite3_close_v2(),
  ** then just leave the mutex and return.
  */
  if( db->eOpenState!=SQLITE_STATE_ZOMBIE || connectionIsBusy(db) ){
    sqlite3_mutex_leave(db->mutex);
    return;
  }

  /* If we reach this point, it means that the database connection has
  ** closed all sqlite3_stmt and sqlite3_backup objects and has been
  ** passed to sqlite3_close (meaning that it is a zombie).  Therefore,
  ** go ahead and free all resources.
  */

  /* If a transaction is open, roll it back. This also ensures that if
  ** any database schemas have been modified by an uncommitted transaction
  ** they are reset. And that the required b-tree mutex is held to make
  ** the pager rollback and schema reset an atomic operation. */
  sqlite3RollbackAll(db, SQLITE_OK);

  /* Free any outstanding Savepoint structures. */
  sqlite3CloseSavepoints(db);

  /* Close all database connections */
  for(j=0; j<db->nDb; j++){
    struct Db *pDb = &db->aDb[j];
    if( pDb->pBt ){
      sqlite3BtreeClose(pDb->pBt);
      pDb->pBt = 0;
      if( j!=1 ){
        pDb->pSchema = 0;
      }
    }
  }
  /* Clear the TEMP schema separately and last */
  if( db->aDb[1].pSchema ){
    sqlite3SchemaClear(db->aDb[1].pSchema);
  }
  sqlite3VtabUnlockList(db);

  /* Free up the array of auxiliary databases */
  sqlite3CollapseDatabaseArray(db);
  assert( db->nDb<=2 );
  assert( db->aDb==db->aDbStatic );

  /* Tell the code in notify.c that the connection no longer holds any
  ** locks and does not require any further unlock-notify callbacks.
  */
  sqlite3ConnectionClosed(db);

  for(i=sqliteHashFirst(&db->aFunc); i; i=sqliteHashNext(i)){
    FuncDef *pNext, *p;
    p = sqliteHashData(i);
    do{
      functionDestroy(db, p);
      pNext = p->pNext;
      sqlite3DbFree(db, p);
      p = pNext;
    }while( p );
  }
  sqlite3HashClear(&db->aFunc);
  for(i=sqliteHashFirst(&db->aCollSeq); i; i=sqliteHashNext(i)){
    CollSeq *pColl = (CollSeq *)sqliteHashData(i);
    /* Invoke any destructors registered for collation sequence user data. */
    for(j=0; j<3; j++){
      if( pColl[j].xDel ){
        pColl[j].xDel(pColl[j].pUser);
      }
    }
    sqlite3DbFree(db, pColl);
  }
  sqlite3HashClear(&db->aCollSeq);
#ifndef SQLITE_OMIT_VIRTUALTABLE
  for(i=sqliteHashFirst(&db->aModule); i; i=sqliteHashNext(i)){
    Module *pMod = (Module *)sqliteHashData(i);
    sqlite3VtabEponymousTableClear(db, pMod);
    sqlite3VtabModuleUnref(db, pMod);
  }
  sqlite3HashClear(&db->aModule);
#endif

  sqlite3Error(db, SQLITE_OK); /* Deallocates any cached error strings. */
  sqlite3ValueFree(db->pErr);
  sqlite3CloseExtensions(db);
#if SQLITE_USER_AUTHENTICATION
  sqlite3_free(db->auth.zAuthUser);
  sqlite3_free(db->auth.zAuthPW);
#endif

  db->eOpenState = SQLITE_STATE_ERROR;

  /* The temp-database schema is allocated differently from the other schema
  ** objects (using sqliteMalloc() directly, instead of sqlite3BtreeSchema()).
  ** So it needs to be freed here. Todo: Why not roll the temp schema into
  ** the same sqliteMalloc() as the one that allocates the database 
  ** structure?
  */
  sqlite3DbFree(db, db->aDb[1].pSchema);
  if( db->xAutovacDestr ){
    db->xAutovacDestr(db->pAutovacPagesArg);
  }
  sqlite3_mutex_leave(db->mutex);
  db->eOpenState = SQLITE_STATE_CLOSED;
  sqlite3_mutex_free(db->mutex);
  assert( sqlite3LookasideUsed(db,0)==0 );
  if( db->lookaside.bMalloced ){
    sqlite3_free(db->lookaside.pStart);
  }
  sqlite3_free(db);
}

/*
** Rollback all database files.  If tripCode is not SQLITE_OK, then
** any write cursors are invalidated ("tripped" - as in "tripping a circuit
** breaker") and made to return tripCode if there are any further
** attempts to use that cursor.  Read cursors remain open and valid
** but are "saved" in case the table pages are moved around.
*/
void sqlite3RollbackAll(sqlite3 *db, int tripCode){
  int i;
  int inTrans = 0;
  int schemaChange;
  assert( sqlite3_mutex_held(db->mutex) );
  sqlite3BeginBenignMalloc();

  /* Obtain all b-tree mutexes before making any calls to BtreeRollback(). 
  ** This is important in case the transaction being rolled back has
  ** modified the database schema. If the b-tree mutexes are not taken
  ** here, then another shared-cache connection might sneak in between
  ** the database rollback and schema reset, which can cause false
  ** corruption reports in some cases.  */
  sqlite3BtreeEnterAll(db);
  schemaChange = (db->mDbFlags & DBFLAG_SchemaChange)!=0 && db->init.busy==0;

  for(i=0; i<db->nDb; i++){
    Btree *p = db->aDb[i].pBt;
    if( p ){
      if( sqlite3BtreeTxnState(p)==SQLITE_TXN_WRITE ){
        inTrans = 1;
      }
      sqlite3BtreeRollback(p, tripCode, !schemaChange);
    }
  }
  sqlite3VtabRollback(db);
  sqlite3EndBenignMalloc();

  if( schemaChange ){
    sqlite3ExpirePreparedStatements(db, 0);
    sqlite3ResetAllSchemasOfConnection(db);
  }
  sqlite3BtreeLeaveAll(db);

  /* Any deferred constraint violations have now been resolved. */
  db->nDeferredCons = 0;
  db->nDeferredImmCons = 0;
  db->flags &= ~(u64)(SQLITE_DeferFKs|SQLITE_CorruptRdOnly);

  /* If one has been configured, invoke the rollback-hook callback */
  if( db->xRollbackCallback && (inTrans || !db->autoCommit) ){
    db->xRollbackCallback(db->pRollbackArg);
  }
}

/*
** Return a static string containing the name corresponding to the error code
** specified in the argument.
*/
#if defined(SQLITE_NEED_ERR_NAME)
const char *sqlite3ErrName(int rc){
  const char *zName = 0;
  int i, origRc = rc;
  for(i=0; i<2 && zName==0; i++, rc &= 0xff){
    switch( rc ){
      case SQLITE_OK:                 zName = "SQLITE_OK";                break;
      case SQLITE_ERROR:              zName = "SQLITE_ERROR";             break;
      case SQLITE_ERROR_SNAPSHOT:     zName = "SQLITE_ERROR_SNAPSHOT";    break;
      case SQLITE_INTERNAL:           zName = "SQLITE_INTERNAL";          break;
      case SQLITE_PERM:               zName = "SQLITE_PERM";              break;
      case SQLITE_ABORT:              zName = "SQLITE_ABORT";             break;
      case SQLITE_ABORT_ROLLBACK:     zName = "SQLITE_ABORT_ROLLBACK";    break;
      case SQLITE_BUSY:               zName = "SQLITE_BUSY";              break;
      case SQLITE_BUSY_RECOVERY:      zName = "SQLITE_BUSY_RECOVERY";     break;
      case SQLITE_BUSY_SNAPSHOT:      zName = "SQLITE_BUSY_SNAPSHOT";     break;
      case SQLITE_LOCKED:             zName = "SQLITE_LOCKED";            break;
      case SQLITE_LOCKED_SHAREDCACHE: zName = "SQLITE_LOCKED_SHAREDCACHE";break;
      case SQLITE_NOMEM:              zName = "SQLITE_NOMEM";             break;
      case SQLITE_READONLY:           zName = "SQLITE_READONLY";          break;
      case SQLITE_READONLY_RECOVERY:  zName = "SQLITE_READONLY_RECOVERY"; break;
      case SQLITE_READONLY_CANTINIT:  zName = "SQLITE_READONLY_CANTINIT"; break;
      case SQLITE_READONLY_ROLLBACK:  zName = "SQLITE_READONLY_ROLLBACK"; break;
      case SQLITE_READONLY_DBMOVED:   zName = "SQLITE_READONLY_DBMOVED";  break;
      case SQLITE_READONLY_DIRECTORY: zName = "SQLITE_READONLY_DIRECTORY";break;
      case SQLITE_INTERRUPT:          zName = "SQLITE_INTERRUPT";         break;
      case SQLITE_IOERR:              zName = "SQLITE_IOERR";             break;
      case SQLITE_IOERR_READ:         zName = "SQLITE_IOERR_READ";        break;
      case SQLITE_IOERR_SHORT_READ:   zName = "SQLITE_IOERR_SHORT_READ";  break;
      case SQLITE_IOERR_WRITE:        zName = "SQLITE_IOERR_WRITE";       break;
      case SQLITE_IOERR_FSYNC:        zName = "SQLITE_IOERR_FSYNC";       break;
      case SQLITE_IOERR_DIR_FSYNC:    zName = "SQLITE_IOERR_DIR_FSYNC";   break;
      case SQLITE_IOERR_TRUNCATE:     zName = "SQLITE_IOERR_TRUNCATE";    break;
      case SQLITE_IOERR_FSTAT:        zName = "SQLITE_IOERR_FSTAT";       break;
      case SQLITE_IOERR_UNLOCK:       zName = "SQLITE_IOERR_UNLOCK";      break;
      case SQLITE_IOERR_RDLOCK:       zName = "SQLITE_IOERR_RDLOCK";      break;
      case SQLITE_IOERR_DELETE:       zName = "SQLITE_IOERR_DELETE";      break;
      case SQLITE_IOERR_NOMEM:        zName = "SQLITE_IOERR_NOMEM";       break;
      case SQLITE_IOERR_ACCESS:       zName = "SQLITE_IOERR_ACCESS";      break;
      case SQLITE_IOERR_CHECKRESERVEDLOCK:
                                zName = "SQLITE_IOERR_CHECKRESERVEDLOCK"; break;
      case SQLITE_IOERR_LOCK:         zName = "SQLITE_IOERR_LOCK";        break;
      case SQLITE_IOERR_CLOSE:        zName = "SQLITE_IOERR_CLOSE";       break;
      case SQLITE_IOERR_DIR_CLOSE:    zName = "SQLITE_IOERR_DIR_CLOSE";   break;
      case SQLITE_IOERR_SHMOPEN:      zName = "SQLITE_IOERR_SHMOPEN";     break;
      case SQLITE_IOERR_SHMSIZE:      zName = "SQLITE_IOERR_SHMSIZE";     break;
      case SQLITE_IOERR_SHMLOCK:      zName = "SQLITE_IOERR_SHMLOCK";     break;
      case SQLITE_IOERR_SHMMAP:       zName = "SQLITE_IOERR_SHMMAP";      break;
      case SQLITE_IOERR_SEEK:         zName = "SQLITE_IOERR_SEEK";        break;
      case SQLITE_IOERR_DELETE_NOENT: zName = "SQLITE_IOERR_DELETE_NOENT";break;
      case SQLITE_IOERR_MMAP:         zName = "SQLITE_IOERR_MMAP";        break;
      case SQLITE_IOERR_GETTEMPPATH:  zName = "SQLITE_IOERR_GETTEMPPATH"; break;
      case SQLITE_IOERR_CONVPATH:     zName = "SQLITE_IOERR_CONVPATH";    break;
      case SQLITE_CORRUPT:            zName = "SQLITE_CORRUPT";           break;
      case SQLITE_CORRUPT_VTAB:       zName = "SQLITE_CORRUPT_VTAB";      break;
      case SQLITE_NOTFOUND:           zName = "SQLITE_NOTFOUND";          break;
      case SQLITE_FULL:               zName = "SQLITE_FULL";              break;
      case SQLITE_CANTOPEN:           zName = "SQLITE_CANTOPEN";          break;
      case SQLITE_CANTOPEN_NOTEMPDIR: zName = "SQLITE_CANTOPEN_NOTEMPDIR";break;
      case SQLITE_CANTOPEN_ISDIR:     zName = "SQLITE_CANTOPEN_ISDIR";    break;
      case SQLITE_CANTOPEN_FULLPATH:  zName = "SQLITE_CANTOPEN_FULLPATH"; break;
      case SQLITE_CANTOPEN_CONVPATH:  zName = "SQLITE_CANTOPEN_CONVPATH"; break;
      case SQLITE_CANTOPEN_SYMLINK:   zName = "SQLITE_CANTOPEN_SYMLINK";  break;
      case SQLITE_PROTOCOL:           zName = "SQLITE_PROTOCOL";          break;
      case SQLITE_EMPTY:              zName = "SQLITE_EMPTY";             break;
      case SQLITE_SCHEMA:             zName = "SQLITE_SCHEMA";            break;
      case SQLITE_TOOBIG:             zName = "SQLITE_TOOBIG";            break;
      case SQLITE_CONSTRAINT:         zName = "SQLITE_CONSTRAINT";        break;
      case SQLITE_CONSTRAINT_UNIQUE:  zName = "SQLITE_CONSTRAINT_UNIQUE"; break;
      case SQLITE_CONSTRAINT_TRIGGER: zName = "SQLITE_CONSTRAINT_TRIGGER";break;
      case SQLITE_CONSTRAINT_FOREIGNKEY:
                                zName = "SQLITE_CONSTRAINT_FOREIGNKEY";   break;
      case SQLITE_CONSTRAINT_CHECK:   zName = "SQLITE_CONSTRAINT_CHECK";  break;
      case SQLITE_CONSTRAINT_PRIMARYKEY:
                                zName = "SQLITE_CONSTRAINT_PRIMARYKEY";   break;
      case SQLITE_CONSTRAINT_NOTNULL: zName = "SQLITE_CONSTRAINT_NOTNULL";break;
      case SQLITE_CONSTRAINT_COMMITHOOK:
                                zName = "SQLITE_CONSTRAINT_COMMITHOOK";   break;
      case SQLITE_CONSTRAINT_VTAB:    zName = "SQLITE_CONSTRAINT_VTAB";   break;
      case SQLITE_CONSTRAINT_FUNCTION:
                                zName = "SQLITE_CONSTRAINT_FUNCTION";     break;
      case SQLITE_CONSTRAINT_ROWID:   zName = "SQLITE_CONSTRAINT_ROWID";  break;
      case SQLITE_MISMATCH:           zName = "SQLITE_MISMATCH";          break;
      case SQLITE_MISUSE:             zName = "SQLITE_MISUSE";            break;
      case SQLITE_NOLFS:              zName = "SQLITE_NOLFS";             break;
      case SQLITE_AUTH:               zName = "SQLITE_AUTH";              break;
      case SQLITE_FORMAT:             zName = "SQLITE_FORMAT";            break;
      case SQLITE_RANGE:              zName = "SQLITE_RANGE";             break;
      case SQLITE_NOTADB:             zName = "SQLITE_NOTADB";            break;
      case SQLITE_ROW:                zName = "SQLITE_ROW";               break;
      case SQLITE_NOTICE:             zName = "SQLITE_NOTICE";            break;
      case SQLITE_NOTICE_RECOVER_WAL: zName = "SQLITE_NOTICE_RECOVER_WAL";break;
      case SQLITE_NOTICE_RECOVER_ROLLBACK:
                                zName = "SQLITE_NOTICE_RECOVER_ROLLBACK"; break;
      case SQLITE_NOTICE_RBU:         zName = "SQLITE_NOTICE_RBU"; break;
      case SQLITE_WARNING:            zName = "SQLITE_WARNING";           break;
      case SQLITE_WARNING_AUTOINDEX:  zName = "SQLITE_WARNING_AUTOINDEX"; break;
      case SQLITE_DONE:               zName = "SQLITE_DONE";              break;
    }
  }
  if( zName==0 ){
    static char zBuf[50];
    sqlite3_snprintf(sizeof(zBuf), zBuf, "SQLITE_UNKNOWN(%d)", origRc);
    zName = zBuf;
  }
  return zName;
}
#endif

/*
** Return a static string that describes the kind of error specified in the
** argument.
*/
const char *sqlite3ErrStr(int rc){
  static const char* const aMsg[] = {
    /* SQLITE_OK          */ "not an error",
    /* SQLITE_ERROR       */ "SQL logic error",
    /* SQLITE_INTERNAL    */ 0,
    /* SQLITE_PERM        */ "access permission denied",
    /* SQLITE_ABORT       */ "query aborted",
    /* SQLITE_BUSY        */ "database is locked",
    /* SQLITE_LOCKED      */ "database table is locked",
    /* SQLITE_NOMEM       */ "out of memory",
    /* SQLITE_READONLY    */ "attempt to write a readonly database",
    /* SQLITE_INTERRUPT   */ "interrupted",
    /* SQLITE_IOERR       */ "disk I/O error",
    /* SQLITE_CORRUPT     */ "database disk image is malformed",
    /* SQLITE_NOTFOUND    */ "unknown operation",
    /* SQLITE_FULL        */ "database or disk is full",
    /* SQLITE_CANTOPEN    */ "unable to open database file",
    /* SQLITE_PROTOCOL    */ "locking protocol",
    /* SQLITE_EMPTY       */ 0,
    /* SQLITE_SCHEMA      */ "database schema has changed",
    /* SQLITE_TOOBIG      */ "string or blob too big",
    /* SQLITE_CONSTRAINT  */ "constraint failed",
    /* SQLITE_MISMATCH    */ "datatype mismatch",
    /* SQLITE_MISUSE      */ "bad parameter or other API misuse",
#ifdef SQLITE_DISABLE_LFS
    /* SQLITE_NOLFS       */ "large file support is disabled",
#else
    /* SQLITE_NOLFS       */ 0,
#endif
    /* SQLITE_AUTH        */ "authorization denied",
    /* SQLITE_FORMAT      */ 0,
    /* SQLITE_RANGE       */ "column index out of range",
    /* SQLITE_NOTADB      */ "file is not a database",
    /* SQLITE_NOTICE      */ "notification message",
    /* SQLITE_WARNING     */ "warning message",
  };
  const char *zErr = "unknown error";
  switch( rc ){
    case SQLITE_ABORT_ROLLBACK: {
      zErr = "abort due to ROLLBACK";
      break;
    }
    case SQLITE_ROW: {
      zErr = "another row available";
      break;
    }
    case SQLITE_DONE: {
      zErr = "no more rows available";
      break;
    }
    default: {
      rc &= 0xff;
      if( ALWAYS(rc>=0) && rc<ArraySize(aMsg) && aMsg[rc]!=0 ){
        zErr = aMsg[rc];
      }
      break;
    }
  }
  return zErr;
}

/*
** This routine implements a busy callback that sleeps and tries
** again until a timeout value is reached.  The timeout value is
** an integer number of milliseconds passed in as the first
** argument.
**
** Return non-zero to retry the lock.  Return zero to stop trying
** and cause SQLite to return SQLITE_BUSY.
*/
static int sqliteDefaultBusyCallback(
  void *ptr,               /* Database connection */
  int count                /* Number of times table has been busy */
){
#if SQLITE_OS_WIN || HAVE_USLEEP
  /* This case is for systems that have support for sleeping for fractions of
  ** a second.  Examples:  All windows systems, unix systems with usleep() */
  static const u8 delays[] =
     { 1, 2, 5, 10, 15, 20, 25, 25,  25,  50,  50, 100 };
  static const u8 totals[] =
     { 0, 1, 3,  8, 18, 33, 53, 78, 103, 128, 178, 228 };
# define NDELAY ArraySize(delays)
  sqlite3 *db = (sqlite3 *)ptr;
  int tmout = db->busyTimeout;
  int delay, prior;

  assert( count>=0 );
  if( count < NDELAY ){
    delay = delays[count];
    prior = totals[count];
  }else{
    delay = delays[NDELAY-1];
    prior = totals[NDELAY-1] + delay*(count-(NDELAY-1));
  }
  if( prior + delay > tmout ){
    delay = tmout - prior;
    if( delay<=0 ) return 0;
  }
  sqlite3OsSleep(db->pVfs, delay*1000);
  return 1;
#else
  /* This case for unix systems that lack usleep() support.  Sleeping
  ** must be done in increments of whole seconds */
  sqlite3 *db = (sqlite3 *)ptr;
  int tmout = ((sqlite3 *)ptr)->busyTimeout;
  if( (count+1)*1000 > tmout ){
    return 0;
  }
  sqlite3OsSleep(db->pVfs, 1000000);
  return 1;
#endif
}

/*
** Invoke the given busy handler.
**
** This routine is called when an operation failed to acquire a
** lock on VFS file pFile.
**
** If this routine returns non-zero, the lock is retried.  If it
** returns 0, the operation aborts with an SQLITE_BUSY error.
*/
int sqlite3InvokeBusyHandler(BusyHandler *p){
  int rc;
  if( p->xBusyHandler==0 || p->nBusy<0 ) return 0;
  rc = p->xBusyHandler(p->pBusyArg, p->nBusy);
  if( rc==0 ){
    p->nBusy = -1;
  }else{
    p->nBusy++;
  }
  return rc; 
}

/*
** This routine sets the busy callback for an Sqlite database to the
** given callback function with the given argument.
*/
int sqlite3_busy_handler(
  sqlite3 *db,
  int (*xBusy)(void*,int),
  void *pArg
){
#ifdef SQLITE_ENABLE_API_ARMOR
  if( !sqlite3SafetyCheckOk(db) ) return SQLITE_MISUSE_BKPT;
#endif
  sqlite3_mutex_enter(db->mutex);
  db->busyHandler.xBusyHandler = xBusy;
  db->busyHandler.pBusyArg = pArg;
  db->busyHandler.nBusy = 0;
  db->busyTimeout = 0;
  sqlite3_mutex_leave(db->mutex);
  return SQLITE_OK;
}

#ifndef SQLITE_OMIT_PROGRESS_CALLBACK
/*
** This routine sets the progress callback for an Sqlite database to the
** given callback function with the given argument. The progress callback will
** be invoked every nOps opcodes.
*/
void sqlite3_progress_handler(
  sqlite3 *db, 
  int nOps,
  int (*xProgress)(void*), 
  void *pArg
){
#ifdef SQLITE_ENABLE_API_ARMOR
  if( !sqlite3SafetyCheckOk(db) ){
    (void)SQLITE_MISUSE_BKPT;
    return;
  }
#endif
  sqlite3_mutex_enter(db->mutex);
  if( nOps>0 ){
    db->xProgress = xProgress;
    db->nProgressOps = (unsigned)nOps;
    db->pProgressArg = pArg;
  }else{
    db->xProgress = 0;
    db->nProgressOps = 0;
    db->pProgressArg = 0;
  }
  sqlite3_mutex_leave(db->mutex);
}
#endif


/*
** This routine installs a default busy handler that waits for the
** specified number of milliseconds before returning 0.
*/
int sqlite3_busy_timeout(sqlite3 *db, int ms){
#ifdef SQLITE_ENABLE_API_ARMOR
  if( !sqlite3SafetyCheckOk(db) ) return SQLITE_MISUSE_BKPT;
#endif
  if( ms>0 ){
    sqlite3_busy_handler(db, (int(*)(void*,int))sqliteDefaultBusyCallback,
                             (void*)db);
    db->busyTimeout = ms;
  }else{
    sqlite3_busy_handler(db, 0, 0);
  }
  return SQLITE_OK;
}

/*
** Cause any pending operation to stop at its earliest opportunity.
*/
void sqlite3_interrupt(sqlite3 *db){
#ifdef SQLITE_ENABLE_API_ARMOR
  if( !sqlite3SafetyCheckOk(db)
   && (db==0 || db->eOpenState!=SQLITE_STATE_ZOMBIE)
  ){
    (void)SQLITE_MISUSE_BKPT;
    return;
  }
#endif
  AtomicStore(&db->u1.isInterrupted, 1);
}

/*
** Return true or false depending on whether or not an interrupt is
** pending on connection db.
*/
int sqlite3_is_interrupted(sqlite3 *db){
#ifdef SQLITE_ENABLE_API_ARMOR
  if( !sqlite3SafetyCheckOk(db)
   && (db==0 || db->eOpenState!=SQLITE_STATE_ZOMBIE)
  ){
    (void)SQLITE_MISUSE_BKPT;
    return 0;
  }
#endif
  return AtomicLoad(&db->u1.isInterrupted)!=0;
}

/*
** This function is exactly the same as sqlite3_create_function(), except
** that it is designed to be called by internal code. The difference is
** that if a malloc() fails in sqlite3_create_function(), an error code
** is returned and the mallocFailed flag cleared. 
*/
int sqlite3CreateFunc(
  sqlite3 *db,
  const char *zFunctionName,
  int nArg,
  int enc,
  void *pUserData,
  void (*xSFunc)(sqlite3_context*,int,sqlite3_value **),
  void (*xStep)(sqlite3_context*,int,sqlite3_value **),
  void (*xFinal)(sqlite3_context*),
  void (*xValue)(sqlite3_context*),
  void (*xInverse)(sqlite3_context*,int,sqlite3_value **),
  FuncDestructor *pDestructor
){
  FuncDef *p;
  int extraFlags;

  assert( sqlite3_mutex_held(db->mutex) );
  assert( xValue==0 || xSFunc==0 );
  if( zFunctionName==0                /* Must have a valid name */
   || (xSFunc!=0 && xFinal!=0)        /* Not both xSFunc and xFinal */
   || ((xFinal==0)!=(xStep==0))       /* Both or neither of xFinal and xStep */
   || ((xValue==0)!=(xInverse==0))    /* Both or neither of xValue, xInverse */
   || (nArg<-1 || nArg>SQLITE_MAX_FUNCTION_ARG)
   || (255<sqlite3Strlen30(zFunctionName))
  ){
    return SQLITE_MISUSE_BKPT;
  }

  assert( SQLITE_FUNC_CONSTANT==SQLITE_DETERMINISTIC );
  assert( SQLITE_FUNC_DIRECT==SQLITE_DIRECTONLY );
  extraFlags = enc &  (SQLITE_DETERMINISTIC|SQLITE_DIRECTONLY|
                       SQLITE_SUBTYPE|SQLITE_INNOCUOUS);
  enc &= (SQLITE_FUNC_ENCMASK|SQLITE_ANY);

  /* The SQLITE_INNOCUOUS flag is the same bit as SQLITE_FUNC_UNSAFE.  But
  ** the meaning is inverted.  So flip the bit. */
  assert( SQLITE_FUNC_UNSAFE==SQLITE_INNOCUOUS );
  extraFlags ^= SQLITE_FUNC_UNSAFE;  /* tag-20230109-1 */

  
#ifndef SQLITE_OMIT_UTF16
  /* If SQLITE_UTF16 is specified as the encoding type, transform this
  ** to one of SQLITE_UTF16LE or SQLITE_UTF16BE using the
  ** SQLITE_UTF16NATIVE macro. SQLITE_UTF16 is not used internally.
  **
  ** If SQLITE_ANY is specified, add three versions of the function
  ** to the hash table.
  */
  switch( enc ){
    case SQLITE_UTF16:
      enc = SQLITE_UTF16NATIVE;
      break;
    case SQLITE_ANY: {
      int rc;
      rc = sqlite3CreateFunc(db, zFunctionName, nArg,
           (SQLITE_UTF8|extraFlags)^SQLITE_FUNC_UNSAFE, /* tag-20230109-1 */
           pUserData, xSFunc, xStep, xFinal, xValue, xInverse, pDestructor);
      if( rc==SQLITE_OK ){
        rc = sqlite3CreateFunc(db, zFunctionName, nArg,
             (SQLITE_UTF16LE|extraFlags)^SQLITE_FUNC_UNSAFE, /* tag-20230109-1*/
             pUserData, xSFunc, xStep, xFinal, xValue, xInverse, pDestructor);
      }
      if( rc!=SQLITE_OK ){
        return rc;
      }
      enc = SQLITE_UTF16BE;
      break;
    }
    case SQLITE_UTF8:
    case SQLITE_UTF16LE:
    case SQLITE_UTF16BE:
      break;
    default:
      enc = SQLITE_UTF8;
      break;
  }
#else
  enc = SQLITE_UTF8;
#endif
  
  /* Check if an existing function is being overridden or deleted. If so,
  ** and there are active VMs, then return SQLITE_BUSY. If a function
  ** is being overridden/deleted but there are no active VMs, allow the
  ** operation to continue but invalidate all precompiled statements.
  */
  p = sqlite3FindFunction(db, zFunctionName, nArg, (u8)enc, 0);
  if( p && (p->funcFlags & SQLITE_FUNC_ENCMASK)==(u32)enc && p->nArg==nArg ){
    if( db->nVdbeActive ){
      sqlite3ErrorWithMsg(db, SQLITE_BUSY, 
        "unable to delete/modify user-function due to active statements");
      assert( !db->mallocFailed );
      return SQLITE_BUSY;
    }else{
      sqlite3ExpirePreparedStatements(db, 0);
    }
  }else if( xSFunc==0 && xFinal==0 ){
    /* Trying to delete a function that does not exist.  This is a no-op.
    ** https://sqlite.org/forum/forumpost/726219164b */
    return SQLITE_OK;
  }

  p = sqlite3FindFunction(db, zFunctionName, nArg, (u8)enc, 1);
  assert(p || db->mallocFailed);
  if( !p ){
    return SQLITE_NOMEM_BKPT;
  }

  /* If an older version of the function with a configured destructor is
  ** being replaced invoke the destructor function here. */
  functionDestroy(db, p);

  if( pDestructor ){
    pDestructor->nRef++;
  }
  p->u.pDestructor = pDestructor;
  p->funcFlags = (p->funcFlags & SQLITE_FUNC_ENCMASK) | extraFlags;
  testcase( p->funcFlags & SQLITE_DETERMINISTIC );
  testcase( p->funcFlags & SQLITE_DIRECTONLY );
  p->xSFunc = xSFunc ? xSFunc : xStep;
  p->xFinalize = xFinal;
  p->xValue = xValue;
  p->xInverse = xInverse;
  p->pUserData = pUserData;
  p->nArg = (u16)nArg;
  return SQLITE_OK;
}

/*
** Worker function used by utf-8 APIs that create new functions:
**
**    sqlite3_create_function()
**    sqlite3_create_function_v2()
**    sqlite3_create_window_function()
*/
static int createFunctionApi(
  sqlite3 *db,
  const char *zFunc,
  int nArg,
  int enc,
  void *p,
  void (*xSFunc)(sqlite3_context*,int,sqlite3_value**),
  void (*xStep)(sqlite3_context*,int,sqlite3_value**),
  void (*xFinal)(sqlite3_context*),
  void (*xValue)(sqlite3_context*),
  void (*xInverse)(sqlite3_context*,int,sqlite3_value**),
  void(*xDestroy)(void*)
){
  int rc = SQLITE_ERROR;
  FuncDestructor *pArg = 0;

#ifdef SQLITE_ENABLE_API_ARMOR
  if( !sqlite3SafetyCheckOk(db) ){
    return SQLITE_MISUSE_BKPT;
  }
#endif
  sqlite3_mutex_enter(db->mutex);
  if( xDestroy ){
    pArg = (FuncDestructor *)sqlite3Malloc(sizeof(FuncDestructor));
    if( !pArg ){
      sqlite3OomFault(db);
      xDestroy(p);
      goto out;
    }
    pArg->nRef = 0;
    pArg->xDestroy = xDestroy;
    pArg->pUserData = p;
  }
  rc = sqlite3CreateFunc(db, zFunc, nArg, enc, p, 
      xSFunc, xStep, xFinal, xValue, xInverse, pArg
  );
  if( pArg && pArg->nRef==0 ){
    assert( rc!=SQLITE_OK || (xStep==0 && xFinal==0) );
    xDestroy(p);
    sqlite3_free(pArg);
  }

 out:
  rc = sqlite3ApiExit(db, rc);
  sqlite3_mutex_leave(db->mutex);
  return rc;
}

/*
** Create new user functions.
*/
int sqlite3_create_function(
  sqlite3 *db,
  const char *zFunc,
  int nArg,
  int enc,
  void *p,
  void (*xSFunc)(sqlite3_context*,int,sqlite3_value **),
  void (*xStep)(sqlite3_context*,int,sqlite3_value **),
  void (*xFinal)(sqlite3_context*)
){
  return createFunctionApi(db, zFunc, nArg, enc, p, xSFunc, xStep,
                                    xFinal, 0, 0, 0);
}
int sqlite3_create_function_v2(
  sqlite3 *db,
  const char *zFunc,
  int nArg,
  int enc,
  void *p,
  void (*xSFunc)(sqlite3_context*,int,sqlite3_value **),
  void (*xStep)(sqlite3_context*,int,sqlite3_value **),
  void (*xFinal)(sqlite3_context*),
  void (*xDestroy)(void *)
){
  return createFunctionApi(db, zFunc, nArg, enc, p, xSFunc, xStep,
                                    xFinal, 0, 0, xDestroy);
}
int sqlite3_create_window_function(
  sqlite3 *db,
  const char *zFunc,
  int nArg,
  int enc,
  void *p,
  void (*xStep)(sqlite3_context*,int,sqlite3_value **),
  void (*xFinal)(sqlite3_context*),
  void (*xValue)(sqlite3_context*),
  void (*xInverse)(sqlite3_context*,int,sqlite3_value **),
  void (*xDestroy)(void *)
){
  return createFunctionApi(db, zFunc, nArg, enc, p, 0, xStep,
                                    xFinal, xValue, xInverse, xDestroy);
}

#ifndef SQLITE_OMIT_UTF16
int sqlite3_create_function16(
  sqlite3 *db,
  const void *zFunctionName,
  int nArg,
  int eTextRep,
  void *p,
  void (*xSFunc)(sqlite3_context*,int,sqlite3_value**),
  void (*xStep)(sqlite3_context*,int,sqlite3_value**),
  void (*xFinal)(sqlite3_context*)
){
  int rc;
  char *zFunc8;

#ifdef SQLITE_ENABLE_API_ARMOR
  if( !sqlite3SafetyCheckOk(db) || zFunctionName==0 ) return SQLITE_MISUSE_BKPT;
#endif
  sqlite3_mutex_enter(db->mutex);
  assert( !db->mallocFailed );
  zFunc8 = sqlite3Utf16to8(db, zFunctionName, -1, SQLITE_UTF16NATIVE);
  rc = sqlite3CreateFunc(db, zFunc8, nArg, eTextRep, p, xSFunc,xStep,xFinal,0,0,0);
  sqlite3DbFree(db, zFunc8);
  rc = sqlite3ApiExit(db, rc);
  sqlite3_mutex_leave(db->mutex);
  return rc;
}
#endif


/*
** The following is the implementation of an SQL function that always
** fails with an error message stating that the function is used in the
** wrong context.  The sqlite3_overload_function() API might construct
** SQL function that use this routine so that the functions will exist
** for name resolution but are actually overloaded by the xFindFunction
** method of virtual tables.
*/
static void sqlite3InvalidFunction(
  sqlite3_context *context,  /* The function calling context */
  int NotUsed,               /* Number of arguments to the function */
  sqlite3_value **NotUsed2   /* Value of each argument */
){
  const char *zName = (const char*)sqlite3_user_data(context);
  char *zErr;
  UNUSED_PARAMETER2(NotUsed, NotUsed2);
  zErr = sqlite3_mprintf(
      "unable to use function %s in the requested context", zName);
  sqlite3_result_error(context, zErr, -1);
  sqlite3_free(zErr);
}

/*
** Declare that a function has been overloaded by a virtual table.
**
** If the function already exists as a regular global function, then
** this routine is a no-op.  If the function does not exist, then create
** a new one that always throws a run-time error.  
**
** When virtual tables intend to provide an overloaded function, they
** should call this routine to make sure the global function exists.
** A global function must exist in order for name resolution to work
** properly.
*/
int sqlite3_overload_function(
  sqlite3 *db,
  const char *zName,
  int nArg
){
  int rc;
  char *zCopy;

#ifdef SQLITE_ENABLE_API_ARMOR
  if( !sqlite3SafetyCheckOk(db) || zName==0 || nArg<-2 ){
    return SQLITE_MISUSE_BKPT;
  }
#endif
  sqlite3_mutex_enter(db->mutex);
  rc = sqlite3FindFunction(db, zName, nArg, SQLITE_UTF8, 0)!=0;
  sqlite3_mutex_leave(db->mutex);
  if( rc ) return SQLITE_OK;
  zCopy = sqlite3_mprintf("%s", zName);
  if( zCopy==0 ) return SQLITE_NOMEM;
  return sqlite3_create_function_v2(db, zName, nArg, SQLITE_UTF8,
                           zCopy, sqlite3InvalidFunction, 0, 0, sqlite3_free);
}

#ifndef SQLITE_OMIT_TRACE
/*
** Register a trace function.  The pArg from the previously registered trace
** is returned.  
**
** A NULL trace function means that no tracing is executes.  A non-NULL
** trace is a pointer to a function that is invoked at the start of each
** SQL statement.
*/
#ifndef SQLITE_OMIT_DEPRECATED
void *sqlite3_trace(sqlite3 *db, void(*xTrace)(void*,const char*), void *pArg){
  void *pOld;

#ifdef SQLITE_ENABLE_API_ARMOR
  if( !sqlite3SafetyCheckOk(db) ){
    (void)SQLITE_MISUSE_BKPT;
    return 0;
  }
#endif
  sqlite3_mutex_enter(db->mutex);
  pOld = db->pTraceArg;
  db->mTrace = xTrace ? SQLITE_TRACE_LEGACY : 0;
  db->trace.xLegacy = xTrace;
  db->pTraceArg = pArg;
  sqlite3_mutex_leave(db->mutex);
  return pOld;
}
#endif /* SQLITE_OMIT_DEPRECATED */

/* Register a trace callback using the version-2 interface.
*/
int sqlite3_trace_v2(
  sqlite3 *db,                               /* Trace this connection */
  unsigned mTrace,                           /* Mask of events to be traced */
  int(*xTrace)(unsigned,void*,void*,void*),  /* Callback to invoke */
  void *pArg                                 /* Context */
){
#ifdef SQLITE_ENABLE_API_ARMOR
  if( !sqlite3SafetyCheckOk(db) ){
    return SQLITE_MISUSE_BKPT;
  }
#endif
  sqlite3_mutex_enter(db->mutex);
  if( mTrace==0 ) xTrace = 0;
  if( xTrace==0 ) mTrace = 0;
  db->mTrace = mTrace;
  db->trace.xV2 = xTrace;
  db->pTraceArg = pArg;
  sqlite3_mutex_leave(db->mutex);
  return SQLITE_OK;
}

#ifndef SQLITE_OMIT_DEPRECATED
/*
** Register a profile function.  The pArg from the previously registered 
** profile function is returned.  
**
** A NULL profile function means that no profiling is executes.  A non-NULL
** profile is a pointer to a function that is invoked at the conclusion of
** each SQL statement that is run.
*/
void *sqlite3_profile(
  sqlite3 *db,
  void (*xProfile)(void*,const char*,sqlite_uint64),
  void *pArg
){
  void *pOld;

#ifdef SQLITE_ENABLE_API_ARMOR
  if( !sqlite3SafetyCheckOk(db) ){
    (void)SQLITE_MISUSE_BKPT;
    return 0;
  }
#endif
  sqlite3_mutex_enter(db->mutex);
  pOld = db->pProfileArg;
  db->xProfile = xProfile;
  db->pProfileArg = pArg;
  db->mTrace &= SQLITE_TRACE_NONLEGACY_MASK;
  if( db->xProfile ) db->mTrace |= SQLITE_TRACE_XPROFILE;
  sqlite3_mutex_leave(db->mutex);
  return pOld;
}
#endif /* SQLITE_OMIT_DEPRECATED */
#endif /* SQLITE_OMIT_TRACE */

/*
** Register a function to be invoked when a transaction commits.
** If the invoked function returns non-zero, then the commit becomes a
** rollback.
*/
void *sqlite3_commit_hook(
  sqlite3 *db,              /* Attach the hook to this database */
  int (*xCallback)(void*),  /* Function to invoke on each commit */
  void *pArg                /* Argument to the function */
){
  void *pOld;

#ifdef SQLITE_ENABLE_API_ARMOR
  if( !sqlite3SafetyCheckOk(db) ){
    (void)SQLITE_MISUSE_BKPT;
    return 0;
  }
#endif
  sqlite3_mutex_enter(db->mutex);
  pOld = db->pCommitArg;
  db->xCommitCallback = xCallback;
  db->pCommitArg = pArg;
  sqlite3_mutex_leave(db->mutex);
  return pOld;
}

/*
** Register a callback to be invoked each time a row is updated,
** inserted or deleted using this database connection.
*/
void *sqlite3_update_hook(
  sqlite3 *db,              /* Attach the hook to this database */
  void (*xCallback)(void*,int,char const *,char const *,sqlite_int64),
  void *pArg                /* Argument to the function */
){
  void *pRet;

#ifdef SQLITE_ENABLE_API_ARMOR
  if( !sqlite3SafetyCheckOk(db) ){
    (void)SQLITE_MISUSE_BKPT;
    return 0;
  }
#endif
  sqlite3_mutex_enter(db->mutex);
  pRet = db->pUpdateArg;
  db->xUpdateCallback = xCallback;
  db->pUpdateArg = pArg;
  sqlite3_mutex_leave(db->mutex);
  return pRet;
}

/*
** Register a callback to be invoked each time a transaction is rolled
** back by this database connection.
*/
void *sqlite3_rollback_hook(
  sqlite3 *db,              /* Attach the hook to this database */
  void (*xCallback)(void*), /* Callback function */
  void *pArg                /* Argument to the function */
){
  void *pRet;

#ifdef SQLITE_ENABLE_API_ARMOR
  if( !sqlite3SafetyCheckOk(db) ){
    (void)SQLITE_MISUSE_BKPT;
    return 0;
  }
#endif
  sqlite3_mutex_enter(db->mutex);
  pRet = db->pRollbackArg;
  db->xRollbackCallback = xCallback;
  db->pRollbackArg = pArg;
  sqlite3_mutex_leave(db->mutex);
  return pRet;
}

#ifdef SQLITE_ENABLE_PREUPDATE_HOOK
/*
** Register a callback to be invoked each time a row is updated,
** inserted or deleted using this database connection.
*/
void *sqlite3_preupdate_hook(
  sqlite3 *db,              /* Attach the hook to this database */
  void(*xCallback)(         /* Callback function */
    void*,sqlite3*,int,char const*,char const*,sqlite3_int64,sqlite3_int64),
  void *pArg                /* First callback argument */
){
  void *pRet;
  sqlite3_mutex_enter(db->mutex);
  pRet = db->pPreUpdateArg;
  db->xPreUpdateCallback = xCallback;
  db->pPreUpdateArg = pArg;
  sqlite3_mutex_leave(db->mutex);
  return pRet;
}
#endif /* SQLITE_ENABLE_PREUPDATE_HOOK */

/*
** Register a function to be invoked prior to each autovacuum that
** determines the number of pages to vacuum.
*/
int sqlite3_autovacuum_pages(
  sqlite3 *db,                 /* Attach the hook to this database */
  unsigned int (*xCallback)(void*,const char*,u32,u32,u32), 
  void *pArg,                  /* Argument to the function */
  void (*xDestructor)(void*)   /* Destructor for pArg */
){
#ifdef SQLITE_ENABLE_API_ARMOR
  if( !sqlite3SafetyCheckOk(db) ){
    if( xDestructor ) xDestructor(pArg);
    return SQLITE_MISUSE_BKPT;
  }
#endif
  sqlite3_mutex_enter(db->mutex);
  if( db->xAutovacDestr ){
    db->xAutovacDestr(db->pAutovacPagesArg);
  }
  db->xAutovacPages = xCallback;
  db->pAutovacPagesArg = pArg;
  db->xAutovacDestr = xDestructor;
  sqlite3_mutex_leave(db->mutex);
  return SQLITE_OK;
}


#ifndef SQLITE_OMIT_WAL
/*
** The sqlite3_wal_hook() callback registered by sqlite3_wal_autocheckpoint().
** Invoke sqlite3_wal_checkpoint if the number of frames in the log file
** is greater than sqlite3.pWalArg cast to an integer (the value configured by
** wal_autocheckpoint()).
*/ 
int sqlite3WalDefaultHook(
  void *pClientData,     /* Argument */
  sqlite3 *db,           /* Connection */
  const char *zDb,       /* Database */
  int nFrame             /* Size of WAL */
){
  if( nFrame>=SQLITE_PTR_TO_INT(pClientData) ){
    sqlite3BeginBenignMalloc();
    sqlite3_wal_checkpoint(db, zDb);
    sqlite3EndBenignMalloc();
  }
  return SQLITE_OK;
}
#endif /* SQLITE_OMIT_WAL */

/*
** Configure an sqlite3_wal_hook() callback to automatically checkpoint
** a database after committing a transaction if there are nFrame or
** more frames in the log file. Passing zero or a negative value as the
** nFrame parameter disables automatic checkpoints entirely.
**
** The callback registered by this function replaces any existing callback
** registered using sqlite3_wal_hook(). Likewise, registering a callback
** using sqlite3_wal_hook() disables the automatic checkpoint mechanism
** configured by this function.
*/
int sqlite3_wal_autocheckpoint(sqlite3 *db, int nFrame){
#ifdef SQLITE_OMIT_WAL
  UNUSED_PARAMETER(db);
  UNUSED_PARAMETER(nFrame);
#else
#ifdef SQLITE_ENABLE_API_ARMOR
  if( !sqlite3SafetyCheckOk(db) ) return SQLITE_MISUSE_BKPT;
#endif
  if( nFrame>0 ){
    sqlite3_wal_hook(db, sqlite3WalDefaultHook, SQLITE_INT_TO_PTR(nFrame));
  }else{
    sqlite3_wal_hook(db, 0, 0);
  }
#endif
  return SQLITE_OK;
}

/*
** Register a callback to be invoked each time a transaction is written
** into the write-ahead-log by this database connection.
*/
void *sqlite3_wal_hook(
  sqlite3 *db,                    /* Attach the hook to this db handle */
  int(*xCallback)(void *, sqlite3*, const char*, int),
  void *pArg                      /* First argument passed to xCallback() */
){
#ifndef SQLITE_OMIT_WAL
  void *pRet;
#ifdef SQLITE_ENABLE_API_ARMOR
  if( !sqlite3SafetyCheckOk(db) ){
    (void)SQLITE_MISUSE_BKPT;
    return 0;
  }
#endif
  sqlite3_mutex_enter(db->mutex);
  pRet = db->pWalArg;
  db->xWalCallback = xCallback;
  db->pWalArg = pArg;
  sqlite3_mutex_leave(db->mutex);
  return pRet;
#else
  return 0;
#endif
}

/*
** Checkpoint database zDb.
*/
int sqlite3_wal_checkpoint_v2(
  sqlite3 *db,                    /* Database handle */
  const char *zDb,                /* Name of attached database (or NULL) */
  int eMode,                      /* SQLITE_CHECKPOINT_* value */
  int *pnLog,                     /* OUT: Size of WAL log in frames */
  int *pnCkpt                     /* OUT: Total number of frames checkpointed */
){
#ifdef SQLITE_OMIT_WAL
  return SQLITE_OK;
#else
  int rc;                         /* Return code */
  int iDb;                        /* Schema to checkpoint */

#ifdef SQLITE_ENABLE_API_ARMOR
  if( !sqlite3SafetyCheckOk(db) ) return SQLITE_MISUSE_BKPT;
#endif

  /* Initialize the output variables to -1 in case an error occurs. */
  if( pnLog ) *pnLog = -1;
  if( pnCkpt ) *pnCkpt = -1;

  assert( SQLITE_CHECKPOINT_PASSIVE==0 );
  assert( SQLITE_CHECKPOINT_FULL==1 );
  assert( SQLITE_CHECKPOINT_RESTART==2 );
  assert( SQLITE_CHECKPOINT_TRUNCATE==3 );
  if( eMode<SQLITE_CHECKPOINT_PASSIVE || eMode>SQLITE_CHECKPOINT_TRUNCATE ){
    /* EVIDENCE-OF: R-03996-12088 The M parameter must be a valid checkpoint
    ** mode: */
    return SQLITE_MISUSE;
  }

  sqlite3_mutex_enter(db->mutex);
  if( zDb && zDb[0] ){
    iDb = sqlite3FindDbName(db, zDb);
  }else{
    iDb = SQLITE_MAX_DB;   /* This means process all schemas */
  }
  if( iDb<0 ){
    rc = SQLITE_ERROR;
    sqlite3ErrorWithMsg(db, SQLITE_ERROR, "unknown database: %s", zDb);
  }else{
    db->busyHandler.nBusy = 0;
    rc = sqlite3Checkpoint(db, iDb, eMode, pnLog, pnCkpt);
    sqlite3Error(db, rc);
  }
  rc = sqlite3ApiExit(db, rc);

  /* If there are no active statements, clear the interrupt flag at this
  ** point.  */
  if( db->nVdbeActive==0 ){
    AtomicStore(&db->u1.isInterrupted, 0);
  }

  sqlite3_mutex_leave(db->mutex);
  return rc;
#endif
}


/*
** Checkpoint database zDb. If zDb is NULL, or if the buffer zDb points
** to contains a zero-length string, all attached databases are 
** checkpointed.
*/
int sqlite3_wal_checkpoint(sqlite3 *db, const char *zDb){
  /* EVIDENCE-OF: R-41613-20553 The sqlite3_wal_checkpoint(D,X) is equivalent to
  ** sqlite3_wal_checkpoint_v2(D,X,SQLITE_CHECKPOINT_PASSIVE,0,0). */
  return sqlite3_wal_checkpoint_v2(db,zDb,SQLITE_CHECKPOINT_PASSIVE,0,0);
}

#ifndef SQLITE_OMIT_WAL
/*
** Run a checkpoint on database iDb. This is a no-op if database iDb is
** not currently open in WAL mode.
**
** If a transaction is open on the database being checkpointed, this 
** function returns SQLITE_LOCKED and a checkpoint is not attempted. If 
** an error occurs while running the checkpoint, an SQLite error code is 
** returned (i.e. SQLITE_IOERR). Otherwise, SQLITE_OK.
**
** The mutex on database handle db should be held by the caller. The mutex
** associated with the specific b-tree being checkpointed is taken by
** this function while the checkpoint is running.
**
** If iDb is passed SQLITE_MAX_DB then all attached databases are
** checkpointed. If an error is encountered it is returned immediately -
** no attempt is made to checkpoint any remaining databases.
**
** Parameter eMode is one of SQLITE_CHECKPOINT_PASSIVE, FULL, RESTART
** or TRUNCATE.
*/
int sqlite3Checkpoint(sqlite3 *db, int iDb, int eMode, int *pnLog, int *pnCkpt){
  int rc = SQLITE_OK;             /* Return code */
  int i;                          /* Used to iterate through attached dbs */
  int bBusy = 0;                  /* True if SQLITE_BUSY has been encountered */

  assert( sqlite3_mutex_held(db->mutex) );
  assert( !pnLog || *pnLog==-1 );
  assert( !pnCkpt || *pnCkpt==-1 );
  testcase( iDb==SQLITE_MAX_ATTACHED ); /* See forum post a006d86f72 */
  testcase( iDb==SQLITE_MAX_DB );

  for(i=0; i<db->nDb && rc==SQLITE_OK; i++){
    if( i==iDb || iDb==SQLITE_MAX_DB ){
      rc = sqlite3BtreeCheckpoint(db->aDb[i].pBt, eMode, pnLog, pnCkpt);
      pnLog = 0;
      pnCkpt = 0;
      if( rc==SQLITE_BUSY ){
        bBusy = 1;
        rc = SQLITE_OK;
      }
    }
  }

  return (rc==SQLITE_OK && bBusy) ? SQLITE_BUSY : rc;
}
#endif /* SQLITE_OMIT_WAL */

/*
** This function returns true if main-memory should be used instead of
** a temporary file for transient pager files and statement journals.
** The value returned depends on the value of db->temp_store (runtime
** parameter) and the compile time value of SQLITE_TEMP_STORE. The
** following table describes the relationship between these two values
** and this functions return value.
**
**   SQLITE_TEMP_STORE     db->temp_store     Location of temporary database
**   -----------------     --------------     ------------------------------
**   0                     any                file      (return 0)
**   1                     1                  file      (return 0)
**   1                     2                  memory    (return 1)
**   1                     0                  file      (return 0)
**   2                     1                  file      (return 0)
**   2                     2                  memory    (return 1)
**   2                     0                  memory    (return 1)
**   3                     any                memory    (return 1)
*/
int sqlite3TempInMemory(const sqlite3 *db){
#if SQLITE_TEMP_STORE==1
  return ( db->temp_store==2 );
#endif
#if SQLITE_TEMP_STORE==2
  return ( db->temp_store!=1 );
#endif
#if SQLITE_TEMP_STORE==3
  UNUSED_PARAMETER(db);
  return 1;
#endif
#if SQLITE_TEMP_STORE<1 || SQLITE_TEMP_STORE>3
  UNUSED_PARAMETER(db);
  return 0;
#endif
}

/*
** Return UTF-8 encoded English language explanation of the most recent
** error.
*/
const char *sqlite3_errmsg(sqlite3 *db){
  const char *z;
  if( !db ){
    return sqlite3ErrStr(SQLITE_NOMEM_BKPT);
  }
  if( !sqlite3SafetyCheckSickOrOk(db) ){
    return sqlite3ErrStr(SQLITE_MISUSE_BKPT);
  }
  sqlite3_mutex_enter(db->mutex);
  if( db->mallocFailed ){
    z = sqlite3ErrStr(SQLITE_NOMEM_BKPT);
  }else{
    testcase( db->pErr==0 );
    z = db->errCode ? (char*)sqlite3_value_text(db->pErr) : 0;
    assert( !db->mallocFailed );
    if( z==0 ){
      z = sqlite3ErrStr(db->errCode);
    }
  }
  sqlite3_mutex_leave(db->mutex);
  return z;
}

/*
** Return the byte offset of the most recent error
*/
int sqlite3_error_offset(sqlite3 *db){
  int iOffset = -1;
  if( db && sqlite3SafetyCheckSickOrOk(db) && db->errCode ){
    sqlite3_mutex_enter(db->mutex);
    iOffset = db->errByteOffset;
    sqlite3_mutex_leave(db->mutex);
  }
  return iOffset;
}

#ifndef SQLITE_OMIT_UTF16
/*
** Return UTF-16 encoded English language explanation of the most recent
** error.
*/
const void *sqlite3_errmsg16(sqlite3 *db){
  static const u16 outOfMem[] = {
    'o', 'u', 't', ' ', 'o', 'f', ' ', 'm', 'e', 'm', 'o', 'r', 'y', 0
  };
  static const u16 misuse[] = {
    'b', 'a', 'd', ' ', 'p', 'a', 'r', 'a', 'm', 'e', 't', 'e', 'r', ' ',
    'o', 'r', ' ', 'o', 't', 'h', 'e', 'r', ' ', 'A', 'P', 'I', ' ',
    'm', 'i', 's', 'u', 's', 'e', 0
  };

  const void *z;
  if( !db ){
    return (void *)outOfMem;
  }
  if( !sqlite3SafetyCheckSickOrOk(db) ){
    return (void *)misuse;
  }
  sqlite3_mutex_enter(db->mutex);
  if( db->mallocFailed ){
    z = (void *)outOfMem;
  }else{
    z = sqlite3_value_text16(db->pErr);
    if( z==0 ){
      sqlite3ErrorWithMsg(db, db->errCode, sqlite3ErrStr(db->errCode));
      z = sqlite3_value_text16(db->pErr);
    }
    /* A malloc() may have failed within the call to sqlite3_value_text16()
    ** above. If this is the case, then the db->mallocFailed flag needs to
    ** be cleared before returning. Do this directly, instead of via
    ** sqlite3ApiExit(), to avoid setting the database handle error message.
    */
    sqlite3OomClear(db);
  }
  sqlite3_mutex_leave(db->mutex);
  return z;
}
#endif /* SQLITE_OMIT_UTF16 */

/*
** Return the most recent error code generated by an SQLite routine. If NULL is
** passed to this function, we assume a malloc() failed during sqlite3_open().
*/
int sqlite3_errcode(sqlite3 *db){
  if( db && !sqlite3SafetyCheckSickOrOk(db) ){
    return SQLITE_MISUSE_BKPT;
  }
  if( !db || db->mallocFailed ){
    return SQLITE_NOMEM_BKPT;
  }
  return db->errCode & db->errMask;
}
int sqlite3_extended_errcode(sqlite3 *db){
  if( db && !sqlite3SafetyCheckSickOrOk(db) ){
    return SQLITE_MISUSE_BKPT;
  }
  if( !db || db->mallocFailed ){
    return SQLITE_NOMEM_BKPT;
  }
  return db->errCode;
}
int sqlite3_system_errno(sqlite3 *db){
  return db ? db->iSysErrno : 0;
}  

/*
** Return a string that describes the kind of error specified in the
** argument.  For now, this simply calls the internal sqlite3ErrStr()
** function.
*/
const char *sqlite3_errstr(int rc){
  return sqlite3ErrStr(rc);
}

/*
** Create a new collating function for database "db".  The name is zName
** and the encoding is enc.
*/
static int createCollation(
  sqlite3* db,
  const char *zName, 
  u8 enc,
  void* pCtx,
  int(*xCompare)(void*,int,const void*,int,const void*),
  void(*xDel)(void*)
){
  CollSeq *pColl;
  int enc2;
  
  assert( sqlite3_mutex_held(db->mutex) );

  /* If SQLITE_UTF16 is specified as the encoding type, transform this
  ** to one of SQLITE_UTF16LE or SQLITE_UTF16BE using the
  ** SQLITE_UTF16NATIVE macro. SQLITE_UTF16 is not used internally.
  */
  enc2 = enc;
  testcase( enc2==SQLITE_UTF16 );
  testcase( enc2==SQLITE_UTF16_ALIGNED );
  if( enc2==SQLITE_UTF16 || enc2==SQLITE_UTF16_ALIGNED ){
    enc2 = SQLITE_UTF16NATIVE;
  }
  if( enc2<SQLITE_UTF8 || enc2>SQLITE_UTF16BE ){
    return SQLITE_MISUSE_BKPT;
  }

  /* Check if this call is removing or replacing an existing collation 
  ** sequence. If so, and there are active VMs, return busy. If there
  ** are no active VMs, invalidate any pre-compiled statements.
  */
  pColl = sqlite3FindCollSeq(db, (u8)enc2, zName, 0);
  if( pColl && pColl->xCmp ){
    if( db->nVdbeActive ){
      sqlite3ErrorWithMsg(db, SQLITE_BUSY, 
        "unable to delete/modify collation sequence due to active statements");
      return SQLITE_BUSY;
    }
    sqlite3ExpirePreparedStatements(db, 0);

    /* If collation sequence pColl was created directly by a call to
    ** sqlite3_create_collation, and not generated by synthCollSeq(),
    ** then any copies made by synthCollSeq() need to be invalidated.
    ** Also, collation destructor - CollSeq.xDel() - function may need
    ** to be called.
    */ 
    if( (pColl->enc & ~SQLITE_UTF16_ALIGNED)==enc2 ){
      CollSeq *aColl = sqlite3HashFind(&db->aCollSeq, zName);
      int j;
      for(j=0; j<3; j++){
        CollSeq *p = &aColl[j];
        if( p->enc==pColl->enc ){
          if( p->xDel ){
            p->xDel(p->pUser);
          }
          p->xCmp = 0;
        }
      }
    }
  }

  pColl = sqlite3FindCollSeq(db, (u8)enc2, zName, 1);
  if( pColl==0 ) return SQLITE_NOMEM_BKPT;
  pColl->xCmp = xCompare;
  pColl->pUser = pCtx;
  pColl->xDel = xDel;
  pColl->enc = (u8)(enc2 | (enc & SQLITE_UTF16_ALIGNED));
  sqlite3Error(db, SQLITE_OK);
  return SQLITE_OK;
}


/*
** This array defines hard upper bounds on limit values.  The
** initializer must be kept in sync with the SQLITE_LIMIT_*
** #defines in sqlite3.h.
*/
static const int aHardLimit[] = {
  SQLITE_MAX_LENGTH,
  SQLITE_MAX_SQL_LENGTH,
  SQLITE_MAX_COLUMN,
  SQLITE_MAX_EXPR_DEPTH,
  SQLITE_MAX_COMPOUND_SELECT,
  SQLITE_MAX_VDBE_OP,
  SQLITE_MAX_FUNCTION_ARG,
  SQLITE_MAX_ATTACHED,
  SQLITE_MAX_LIKE_PATTERN_LENGTH,
  SQLITE_MAX_VARIABLE_NUMBER,      /* IMP: R-38091-32352 */
  SQLITE_MAX_TRIGGER_DEPTH,
  SQLITE_MAX_WORKER_THREADS,
};

/*
** Make sure the hard limits are set to reasonable values
*/
#if SQLITE_MAX_LENGTH<100
# error SQLITE_MAX_LENGTH must be at least 100
#endif
#if SQLITE_MAX_SQL_LENGTH<100
# error SQLITE_MAX_SQL_LENGTH must be at least 100
#endif
#if SQLITE_MAX_SQL_LENGTH>SQLITE_MAX_LENGTH
# error SQLITE_MAX_SQL_LENGTH must not be greater than SQLITE_MAX_LENGTH
#endif
#if SQLITE_MAX_COMPOUND_SELECT<2
# error SQLITE_MAX_COMPOUND_SELECT must be at least 2
#endif
#if SQLITE_MAX_VDBE_OP<40
# error SQLITE_MAX_VDBE_OP must be at least 40
#endif
#if SQLITE_MAX_FUNCTION_ARG<0 || SQLITE_MAX_FUNCTION_ARG>127
# error SQLITE_MAX_FUNCTION_ARG must be between 0 and 127
#endif
#if SQLITE_MAX_ATTACHED<0 || SQLITE_MAX_ATTACHED>125
# error SQLITE_MAX_ATTACHED must be between 0 and 125
#endif
#if SQLITE_MAX_LIKE_PATTERN_LENGTH<1
# error SQLITE_MAX_LIKE_PATTERN_LENGTH must be at least 1
#endif
#if SQLITE_MAX_COLUMN>32767
# error SQLITE_MAX_COLUMN must not exceed 32767
#endif
#if SQLITE_MAX_TRIGGER_DEPTH<1
# error SQLITE_MAX_TRIGGER_DEPTH must be at least 1
#endif
#if SQLITE_MAX_WORKER_THREADS<0 || SQLITE_MAX_WORKER_THREADS>50
# error SQLITE_MAX_WORKER_THREADS must be between 0 and 50
#endif


/*
** Change the value of a limit.  Report the old value.
** If an invalid limit index is supplied, report -1.
** Make no changes but still report the old value if the
** new limit is negative.
**
** A new lower limit does not shrink existing constructs.
** It merely prevents new constructs that exceed the limit
** from forming.
*/
int sqlite3_limit(sqlite3 *db, int limitId, int newLimit){
  int oldLimit;

#ifdef SQLITE_ENABLE_API_ARMOR
  if( !sqlite3SafetyCheckOk(db) ){
    (void)SQLITE_MISUSE_BKPT;
    return -1;
  }
#endif

  /* EVIDENCE-OF: R-30189-54097 For each limit category SQLITE_LIMIT_NAME
  ** there is a hard upper bound set at compile-time by a C preprocessor
  ** macro called SQLITE_MAX_NAME. (The "_LIMIT_" in the name is changed to
  ** "_MAX_".)
  */
  assert( aHardLimit[SQLITE_LIMIT_LENGTH]==SQLITE_MAX_LENGTH );
  assert( aHardLimit[SQLITE_LIMIT_SQL_LENGTH]==SQLITE_MAX_SQL_LENGTH );
  assert( aHardLimit[SQLITE_LIMIT_COLUMN]==SQLITE_MAX_COLUMN );
  assert( aHardLimit[SQLITE_LIMIT_EXPR_DEPTH]==SQLITE_MAX_EXPR_DEPTH );
  assert( aHardLimit[SQLITE_LIMIT_COMPOUND_SELECT]==SQLITE_MAX_COMPOUND_SELECT);
  assert( aHardLimit[SQLITE_LIMIT_VDBE_OP]==SQLITE_MAX_VDBE_OP );
  assert( aHardLimit[SQLITE_LIMIT_FUNCTION_ARG]==SQLITE_MAX_FUNCTION_ARG );
  assert( aHardLimit[SQLITE_LIMIT_ATTACHED]==SQLITE_MAX_ATTACHED );
  assert( aHardLimit[SQLITE_LIMIT_LIKE_PATTERN_LENGTH]==
                                               SQLITE_MAX_LIKE_PATTERN_LENGTH );
  assert( aHardLimit[SQLITE_LIMIT_VARIABLE_NUMBER]==SQLITE_MAX_VARIABLE_NUMBER);
  assert( aHardLimit[SQLITE_LIMIT_TRIGGER_DEPTH]==SQLITE_MAX_TRIGGER_DEPTH );
  assert( aHardLimit[SQLITE_LIMIT_WORKER_THREADS]==SQLITE_MAX_WORKER_THREADS );
  assert( SQLITE_LIMIT_WORKER_THREADS==(SQLITE_N_LIMIT-1) );


  if( limitId<0 || limitId>=SQLITE_N_LIMIT ){
    return -1;
  }
  oldLimit = db->aLimit[limitId];
  if( newLimit>=0 ){                   /* IMP: R-52476-28732 */
    if( newLimit>aHardLimit[limitId] ){
      newLimit = aHardLimit[limitId];  /* IMP: R-51463-25634 */
    }else if( newLimit<1 && limitId==SQLITE_LIMIT_LENGTH ){
      newLimit = 1;
    }
    db->aLimit[limitId] = newLimit;
  }
  return oldLimit;                     /* IMP: R-53341-35419 */
}

/*
** This function is used to parse both URIs and non-URI filenames passed by the
** user to API functions sqlite3_open() or sqlite3_open_v2(), and for database
** URIs specified as part of ATTACH statements.
**
** The first argument to this function is the name of the VFS to use (or
** a NULL to signify the default VFS) if the URI does not contain a "vfs=xxx"
** query parameter. The second argument contains the URI (or non-URI filename)
** itself. When this function is called the *pFlags variable should contain
** the default flags to open the database handle with. The value stored in
** *pFlags may be updated before returning if the URI filename contains 
** "cache=xxx" or "mode=xxx" query parameters.
**
** If successful, SQLITE_OK is returned. In this case *ppVfs is set to point to
** the VFS that should be used to open the database file. *pzFile is set to
** point to a buffer containing the name of the file to open.  The value
** stored in *pzFile is a database name acceptable to sqlite3_uri_parameter()
** and is in the same format as names created using sqlite3_create_filename().
** The caller must invoke sqlite3_free_filename() (not sqlite3_free()!) on
** the value returned in *pzFile to avoid a memory leak.
**
** If an error occurs, then an SQLite error code is returned and *pzErrMsg
** may be set to point to a buffer containing an English language error 
** message. It is the responsibility of the caller to eventually release
** this buffer by calling sqlite3_free().
*/
int sqlite3ParseUri(
  const char *zDefaultVfs,        /* VFS to use if no "vfs=xxx" query option */
  const char *zUri,               /* Nul-terminated URI to parse */
  unsigned int *pFlags,           /* IN/OUT: SQLITE_OPEN_XXX flags */
  sqlite3_vfs **ppVfs,            /* OUT: VFS to use */ 
  char **pzFile,                  /* OUT: Filename component of URI */
  char **pzErrMsg                 /* OUT: Error message (if rc!=SQLITE_OK) */
){
  int rc = SQLITE_OK;
  unsigned int flags = *pFlags;
  const char *zVfs = zDefaultVfs;
  char *zFile;
  char c;
  int nUri = sqlite3Strlen30(zUri);

  assert( *pzErrMsg==0 );

  if( ((flags & SQLITE_OPEN_URI)                     /* IMP: R-48725-32206 */
       || AtomicLoad(&sqlite3GlobalConfig.bOpenUri)) /* IMP: R-51689-46548 */
   && nUri>=5 && memcmp(zUri, "file:", 5)==0         /* IMP: R-57884-37496 */
  ){
    char *zOpt;
    int eState;                   /* Parser state when parsing URI */
    int iIn;                      /* Input character index */
    int iOut = 0;                 /* Output character index */
    u64 nByte = nUri+8;           /* Bytes of space to allocate */

    /* Make sure the SQLITE_OPEN_URI flag is set to indicate to the VFS xOpen 
    ** method that there may be extra parameters following the file-name.  */
    flags |= SQLITE_OPEN_URI;

    for(iIn=0; iIn<nUri; iIn++) nByte += (zUri[iIn]=='&');
    zFile = sqlite3_malloc64(nByte);
    if( !zFile ) return SQLITE_NOMEM_BKPT;

    memset(zFile, 0, 4);  /* 4-byte of 0x00 is the start of DB name marker */
    zFile += 4;

    iIn = 5;
#ifdef SQLITE_ALLOW_URI_AUTHORITY
    if( strncmp(zUri+5, "///", 3)==0 ){
      iIn = 7;
      /* The following condition causes URIs with five leading / characters
      ** like file://///host/path to be converted into UNCs like //host/path.
      ** The correct URI for that UNC has only two or four leading / characters
      ** file://host/path or file:////host/path.  But 5 leading slashes is a 
      ** common error, we are told, so we handle it as a special case. */
      if( strncmp(zUri+7, "///", 3)==0 ){ iIn++; }
    }else if( strncmp(zUri+5, "//localhost/", 12)==0 ){
      iIn = 16;
    }
#else
    /* Discard the scheme and authority segments of the URI. */
    if( zUri[5]=='/' && zUri[6]=='/' ){
      iIn = 7;
      while( zUri[iIn] && zUri[iIn]!='/' ) iIn++;
      if( iIn!=7 && (iIn!=16 || memcmp("localhost", &zUri[7], 9)) ){
        *pzErrMsg = sqlite3_mprintf("invalid uri authority: %.*s", 
            iIn-7, &zUri[7]);
        rc = SQLITE_ERROR;
        goto parse_uri_out;
      }
    }
#endif

    /* Copy the filename and any query parameters into the zFile buffer. 
    ** Decode %HH escape codes along the way. 
    **
    ** Within this loop, variable eState may be set to 0, 1 or 2, depending
    ** on the parsing context. As follows:
    **
    **   0: Parsing file-name.
    **   1: Parsing name section of a name=value query parameter.
    **   2: Parsing value section of a name=value query parameter.
    */
    eState = 0;
    while( (c = zUri[iIn])!=0 && c!='#' ){
      iIn++;
      if( c=='%' 
       && sqlite3Isxdigit(zUri[iIn]) 
       && sqlite3Isxdigit(zUri[iIn+1]) 
      ){
        int octet = (sqlite3HexToInt(zUri[iIn++]) << 4);
        octet += sqlite3HexToInt(zUri[iIn++]);

        assert( octet>=0 && octet<256 );
        if( octet==0 ){
#ifndef SQLITE_ENABLE_URI_00_ERROR
          /* This branch is taken when "%00" appears within the URI. In this
          ** case we ignore all text in the remainder of the path, name or
          ** value currently being parsed. So ignore the current character
          ** and skip to the next "?", "=" or "&", as appropriate. */
          while( (c = zUri[iIn])!=0 && c!='#' 
              && (eState!=0 || c!='?')
              && (eState!=1 || (c!='=' && c!='&'))
              && (eState!=2 || c!='&')
          ){
            iIn++;
          }
          continue;
#else
          /* If ENABLE_URI_00_ERROR is defined, "%00" in a URI is an error. */
          *pzErrMsg = sqlite3_mprintf("unexpected %%00 in uri");
          rc = SQLITE_ERROR;
          goto parse_uri_out;
#endif
        }
        c = octet;
      }else if( eState==1 && (c=='&' || c=='=') ){
        if( zFile[iOut-1]==0 ){
          /* An empty option name. Ignore this option altogether. */
          while( zUri[iIn] && zUri[iIn]!='#' && zUri[iIn-1]!='&' ) iIn++;
          continue;
        }
        if( c=='&' ){
          zFile[iOut++] = '\0';
        }else{
          eState = 2;
        }
        c = 0;
      }else if( (eState==0 && c=='?') || (eState==2 && c=='&') ){
        c = 0;
        eState = 1;
      }
      zFile[iOut++] = c;
    }
    if( eState==1 ) zFile[iOut++] = '\0';
    memset(zFile+iOut, 0, 4); /* end-of-options + empty journal filenames */

    /* Check if there were any options specified that should be interpreted 
    ** here. Options that are interpreted here include "vfs" and those that
    ** correspond to flags that may be passed to the sqlite3_open_v2()
    ** method. */
    zOpt = &zFile[sqlite3Strlen30(zFile)+1];
    while( zOpt[0] ){
      int nOpt = sqlite3Strlen30(zOpt);
      char *zVal = &zOpt[nOpt+1];
      int nVal = sqlite3Strlen30(zVal);

      if( nOpt==3 && memcmp("vfs", zOpt, 3)==0 ){
        zVfs = zVal;
      }else{
        struct OpenMode {
          const char *z;
          int mode;
        } *aMode = 0;
        char *zModeType = 0;
        int mask = 0;
        int limit = 0;

        if( nOpt==5 && memcmp("cache", zOpt, 5)==0 ){
          static struct OpenMode aCacheMode[] = {
            { "shared",  SQLITE_OPEN_SHAREDCACHE },
            { "private", SQLITE_OPEN_PRIVATECACHE },
            { 0, 0 }
          };

          mask = SQLITE_OPEN_SHAREDCACHE|SQLITE_OPEN_PRIVATECACHE;
          aMode = aCacheMode;
          limit = mask;
          zModeType = "cache";
        }
        if( nOpt==4 && memcmp("mode", zOpt, 4)==0 ){
          static struct OpenMode aOpenMode[] = {
            { "ro",  SQLITE_OPEN_READONLY },
            { "rw",  SQLITE_OPEN_READWRITE }, 
            { "rwc", SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE },
            { "memory", SQLITE_OPEN_MEMORY },
            { 0, 0 }
          };

          mask = SQLITE_OPEN_READONLY | SQLITE_OPEN_READWRITE
                   | SQLITE_OPEN_CREATE | SQLITE_OPEN_MEMORY;
          aMode = aOpenMode;
          limit = mask & flags;
          zModeType = "access";
        }

        if( aMode ){
          int i;
          int mode = 0;
          for(i=0; aMode[i].z; i++){
            const char *z = aMode[i].z;
            if( nVal==sqlite3Strlen30(z) && 0==memcmp(zVal, z, nVal) ){
              mode = aMode[i].mode;
              break;
            }
          }
          if( mode==0 ){
            *pzErrMsg = sqlite3_mprintf("no such %s mode: %s", zModeType, zVal);
            rc = SQLITE_ERROR;
            goto parse_uri_out;
          }
          if( (mode & ~SQLITE_OPEN_MEMORY)>limit ){
            *pzErrMsg = sqlite3_mprintf("%s mode not allowed: %s",
                                        zModeType, zVal);
            rc = SQLITE_PERM;
            goto parse_uri_out;
          }
          flags = (flags & ~mask) | mode;
        }
      }

      zOpt = &zVal[nVal+1];
    }

  }else{
    zFile = sqlite3_malloc64(nUri+8);
    if( !zFile ) return SQLITE_NOMEM_BKPT;
    memset(zFile, 0, 4);
    zFile += 4;
    if( nUri ){
      memcpy(zFile, zUri, nUri);
    }
    memset(zFile+nUri, 0, 4);
    flags &= ~SQLITE_OPEN_URI;
  }

  *ppVfs = sqlite3_vfs_find(zVfs);
  if( *ppVfs==0 ){
    *pzErrMsg = sqlite3_mprintf("no such vfs: %s", zVfs);
    rc = SQLITE_ERROR;
  }
 parse_uri_out:
  if( rc!=SQLITE_OK ){
    sqlite3_free_filename(zFile);
    zFile = 0;
  }
  *pFlags = flags;
  *pzFile = zFile;
  return rc;
}

/*
** This routine does the core work of extracting URI parameters from a
** database filename for the sqlite3_uri_parameter() interface.
*/
static const char *uriParameter(const char *zFilename, const char *zParam){
  zFilename += sqlite3Strlen30(zFilename) + 1;
  while( ALWAYS(zFilename!=0) && zFilename[0] ){
    int x = strcmp(zFilename, zParam);
    zFilename += sqlite3Strlen30(zFilename) + 1;
    if( x==0 ) return zFilename;
    zFilename += sqlite3Strlen30(zFilename) + 1;
  }
  return 0;
}



/*
** This routine does the work of opening a database on behalf of
** sqlite3_open() and sqlite3_open16(). The database filename "zFilename"  
** is UTF-8 encoded.
*/
static int openDatabase(
  const char *zFilename, /* Database filename UTF-8 encoded */
  sqlite3 **ppDb,        /* OUT: Returned database handle */
  unsigned int flags,    /* Operational flags */
  const char *zVfs       /* Name of the VFS to use */
){
  sqlite3 *db;                    /* Store allocated handle here */
  int rc;                         /* Return code */
  int isThreadsafe;               /* True for threadsafe connections */
  char *zOpen = 0;                /* Filename argument to pass to BtreeOpen() */
  char *zErrMsg = 0;              /* Error message from sqlite3ParseUri() */
  int i;                          /* Loop counter */

#ifdef SQLITE_ENABLE_API_ARMOR
  if( ppDb==0 ) return SQLITE_MISUSE_BKPT;
#endif
  *ppDb = 0;
#ifndef SQLITE_OMIT_AUTOINIT
  rc = sqlite3_initialize();
  if( rc ) return rc;
#endif

  if( sqlite3GlobalConfig.bCoreMutex==0 ){
    isThreadsafe = 0;
  }else if( flags & SQLITE_OPEN_NOMUTEX ){
    isThreadsafe = 0;
  }else if( flags & SQLITE_OPEN_FULLMUTEX ){
    isThreadsafe = 1;
  }else{
    isThreadsafe = sqlite3GlobalConfig.bFullMutex;
  }

  if( flags & SQLITE_OPEN_PRIVATECACHE ){
    flags &= ~SQLITE_OPEN_SHAREDCACHE;
  }else if( sqlite3GlobalConfig.sharedCacheEnabled ){
    flags |= SQLITE_OPEN_SHAREDCACHE;
  }

  /* Remove harmful bits from the flags parameter
  **
  ** The SQLITE_OPEN_NOMUTEX and SQLITE_OPEN_FULLMUTEX flags were
  ** dealt with in the previous code block.  Besides these, the only
  ** valid input flags for sqlite3_open_v2() are SQLITE_OPEN_READONLY,
  ** SQLITE_OPEN_READWRITE, SQLITE_OPEN_CREATE, SQLITE_OPEN_SHAREDCACHE,
  ** SQLITE_OPEN_PRIVATECACHE, SQLITE_OPEN_EXRESCODE, and some reserved
  ** bits.  Silently mask off all other flags.
  */
  flags &=  ~( SQLITE_OPEN_DELETEONCLOSE |
               SQLITE_OPEN_EXCLUSIVE |
               SQLITE_OPEN_MAIN_DB |
               SQLITE_OPEN_TEMP_DB | 
               SQLITE_OPEN_TRANSIENT_DB | 
               SQLITE_OPEN_MAIN_JOURNAL | 
               SQLITE_OPEN_TEMP_JOURNAL | 
               SQLITE_OPEN_SUBJOURNAL | 
               SQLITE_OPEN_SUPER_JOURNAL |
               SQLITE_OPEN_NOMUTEX |
               SQLITE_OPEN_FULLMUTEX |
               SQLITE_OPEN_WAL
             );

  /* Allocate the sqlite data structure */
  db = sqlite3MallocZero( sizeof(sqlite3) );
  if( db==0 ) goto opendb_out;
  if( isThreadsafe 
#ifdef SQLITE_ENABLE_MULTITHREADED_CHECKS
   || sqlite3GlobalConfig.bCoreMutex
#endif
  ){
    db->mutex = sqlite3MutexAlloc(SQLITE_MUTEX_RECURSIVE);
    if( db->mutex==0 ){
      sqlite3_free(db);
      db = 0;
      goto opendb_out;
    }
    if( isThreadsafe==0 ){
      sqlite3MutexWarnOnContention(db->mutex);
    }
  }
  sqlite3_mutex_enter(db->mutex);
  db->errMask = (flags & SQLITE_OPEN_EXRESCODE)!=0 ? 0xffffffff : 0xff;
  db->nDb = 2;
  db->eOpenState = SQLITE_STATE_BUSY;
  db->aDb = db->aDbStatic;
  db->lookaside.bDisable = 1;
  db->lookaside.sz = 0;

  assert( sizeof(db->aLimit)==sizeof(aHardLimit) );
  memcpy(db->aLimit, aHardLimit, sizeof(db->aLimit));
  db->aLimit[SQLITE_LIMIT_WORKER_THREADS] = SQLITE_DEFAULT_WORKER_THREADS;
  db->autoCommit = 1;
  db->nextAutovac = -1;
  db->szMmap = sqlite3GlobalConfig.szMmap;
  db->nextPagesize = 0;
  db->init.azInit = sqlite3StdType; /* Any array of string ptrs will do */
#ifdef SQLITE_ENABLE_SORTER_MMAP
  /* Beginning with version 3.37.0, using the VFS xFetch() API to memory-map 
  ** the temporary files used to do external sorts (see code in vdbesort.c)
  ** is disabled. It can still be used either by defining
  ** SQLITE_ENABLE_SORTER_MMAP at compile time or by using the
  ** SQLITE_TESTCTRL_SORTER_MMAP test-control at runtime. */
  db->nMaxSorterMmap = 0x7FFFFFFF;
#endif
  db->flags |= SQLITE_ShortColNames
                 | SQLITE_EnableTrigger
                 | SQLITE_EnableView
                 | SQLITE_CacheSpill
#if !defined(SQLITE_TRUSTED_SCHEMA) || SQLITE_TRUSTED_SCHEMA+0!=0
                 | SQLITE_TrustedSchema
#endif
/* The SQLITE_DQS compile-time option determines the default settings
** for SQLITE_DBCONFIG_DQS_DDL and SQLITE_DBCONFIG_DQS_DML.
**
**    SQLITE_DQS     SQLITE_DBCONFIG_DQS_DDL    SQLITE_DBCONFIG_DQS_DML
**    ----------     -----------------------    -----------------------
**     undefined               on                          on   
**         3                   on                          on
**         2                   on                         off
**         1                  off                          on
**         0                  off                         off
**
** Legacy behavior is 3 (double-quoted string literals are allowed anywhere)
** and so that is the default.  But developers are encouranged to use
** -DSQLITE_DQS=0 (best) or -DSQLITE_DQS=1 (second choice) if possible.
*/
#if !defined(SQLITE_DQS)
# define SQLITE_DQS 3
#endif
#if (SQLITE_DQS&1)==1
                 | SQLITE_DqsDML
#endif
#if (SQLITE_DQS&2)==2
                 | SQLITE_DqsDDL
#endif

#if !defined(SQLITE_DEFAULT_AUTOMATIC_INDEX) || SQLITE_DEFAULT_AUTOMATIC_INDEX
                 | SQLITE_AutoIndex
#endif
#if SQLITE_DEFAULT_CKPTFULLFSYNC
                 | SQLITE_CkptFullFSync
#endif
#if SQLITE_DEFAULT_FILE_FORMAT<4
                 | SQLITE_LegacyFileFmt
#endif
#ifdef SQLITE_ENABLE_LOAD_EXTENSION
                 | SQLITE_LoadExtension
#endif
#if SQLITE_DEFAULT_RECURSIVE_TRIGGERS
                 | SQLITE_RecTriggers
#endif
#if defined(SQLITE_DEFAULT_FOREIGN_KEYS) && SQLITE_DEFAULT_FOREIGN_KEYS
                 | SQLITE_ForeignKeys
#endif
#if defined(SQLITE_REVERSE_UNORDERED_SELECTS)
                 | SQLITE_ReverseOrder
#endif
#if defined(SQLITE_ENABLE_OVERSIZE_CELL_CHECK)
                 | SQLITE_CellSizeCk
#endif
#if defined(SQLITE_ENABLE_FTS3_TOKENIZER)
                 | SQLITE_Fts3Tokenizer
#endif
#if defined(SQLITE_ENABLE_QPSG)
                 | SQLITE_EnableQPSG
#endif
#if defined(SQLITE_DEFAULT_DEFENSIVE)
                 | SQLITE_Defensive
#endif
#if defined(SQLITE_DEFAULT_LEGACY_ALTER_TABLE)
                 | SQLITE_LegacyAlter
#endif
#if defined(SQLITE_ENABLE_STMT_SCANSTATUS)
                 | SQLITE_StmtScanStatus
#endif
      ;
  sqlite3HashInit(&db->aCollSeq);
#ifndef SQLITE_OMIT_VIRTUALTABLE
  sqlite3HashInit(&db->aModule);
#endif

  /* Add the default collation sequence BINARY. BINARY works for both UTF-8
  ** and UTF-16, so add a version for each to avoid any unnecessary
  ** conversions. The only error that can occur here is a malloc() failure.
  **
  ** EVIDENCE-OF: R-52786-44878 SQLite defines three built-in collating
  ** functions:
  */
  createCollation(db, sqlite3StrBINARY, SQLITE_UTF8, 0, binCollFunc, 0);
  createCollation(db, sqlite3StrBINARY, SQLITE_UTF16BE, 0, binCollFunc, 0);
  createCollation(db, sqlite3StrBINARY, SQLITE_UTF16LE, 0, binCollFunc, 0);
  createCollation(db, "NOCASE", SQLITE_UTF8, 0, nocaseCollatingFunc, 0);
  createCollation(db, "RTRIM", SQLITE_UTF8, 0, rtrimCollFunc, 0);
  if( db->mallocFailed ){
    goto opendb_out;
  }

#if SQLITE_OS_UNIX && defined(SQLITE_OS_KV_OPTIONAL)
  /* Process magic filenames ":localStorage:" and ":sessionStorage:" */
  if( zFilename && zFilename[0]==':' ){
    if( strcmp(zFilename, ":localStorage:")==0 ){
      zFilename = "file:local?vfs=kvvfs";
      flags |= SQLITE_OPEN_URI;
    }else if( strcmp(zFilename, ":sessionStorage:")==0 ){
      zFilename = "file:session?vfs=kvvfs";
      flags |= SQLITE_OPEN_URI;
    }
  }
#endif /* SQLITE_OS_UNIX && defined(SQLITE_OS_KV_OPTIONAL) */

  /* Parse the filename/URI argument
  **
  ** Only allow sensible combinations of bits in the flags argument.  
  ** Throw an error if any non-sense combination is used.  If we
  ** do not block illegal combinations here, it could trigger
  ** assert() statements in deeper layers.  Sensible combinations
  ** are:
  **
  **  1:  SQLITE_OPEN_READONLY
  **  2:  SQLITE_OPEN_READWRITE
  **  6:  SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE
  */
  db->openFlags = flags;
  assert( SQLITE_OPEN_READONLY  == 0x01 );
  assert( SQLITE_OPEN_READWRITE == 0x02 );
  assert( SQLITE_OPEN_CREATE    == 0x04 );
  testcase( (1<<(flags&7))==0x02 ); /* READONLY */
  testcase( (1<<(flags&7))==0x04 ); /* READWRITE */
  testcase( (1<<(flags&7))==0x40 ); /* READWRITE | CREATE */
  if( ((1<<(flags&7)) & 0x46)==0 ){
    rc = SQLITE_MISUSE_BKPT;  /* IMP: R-18321-05872 */
  }else{
    rc = sqlite3ParseUri(zVfs, zFilename, &flags, &db->pVfs, &zOpen, &zErrMsg);
  }
  if( rc!=SQLITE_OK ){
    if( rc==SQLITE_NOMEM ) sqlite3OomFault(db);
    sqlite3ErrorWithMsg(db, rc, zErrMsg ? "%s" : 0, zErrMsg);
    sqlite3_free(zErrMsg);
    goto opendb_out;
  }
  assert( db->pVfs!=0 );
#if SQLITE_OS_KV || defined(SQLITE_OS_KV_OPTIONAL)
  if( sqlite3_stricmp(db->pVfs->zName, "kvvfs")==0 ){
    db->temp_store = 2;
  }
#endif

  /* Open the backend database driver */
  rc = sqlite3BtreeOpen(db->pVfs, zOpen, db, &db->aDb[0].pBt, 0,
                        flags | SQLITE_OPEN_MAIN_DB);
  if( rc!=SQLITE_OK ){
    if( rc==SQLITE_IOERR_NOMEM ){
      rc = SQLITE_NOMEM_BKPT;
    }
    sqlite3Error(db, rc);
    goto opendb_out;
  }
  sqlite3BtreeEnter(db->aDb[0].pBt);
  db->aDb[0].pSchema = sqlite3SchemaGet(db, db->aDb[0].pBt);
  if( !db->mallocFailed ){
    sqlite3SetTextEncoding(db, SCHEMA_ENC(db));
  }
  sqlite3BtreeLeave(db->aDb[0].pBt);
  db->aDb[1].pSchema = sqlite3SchemaGet(db, 0);

  /* The default safety_level for the main database is FULL; for the temp
  ** database it is OFF. This matches the pager layer defaults.  
  */
  db->aDb[0].zDbSName = "main";
  db->aDb[0].safety_level = SQLITE_DEFAULT_SYNCHRONOUS+1;
  db->aDb[1].zDbSName = "temp";
  db->aDb[1].safety_level = PAGER_SYNCHRONOUS_OFF;

  db->eOpenState = SQLITE_STATE_OPEN;
  if( db->mallocFailed ){
    goto opendb_out;
  }

  /* Register all built-in functions, but do not attempt to read the
  ** database schema yet. This is delayed until the first time the database
  ** is accessed.
  */
  sqlite3Error(db, SQLITE_OK);
  sqlite3RegisterPerConnectionBuiltinFunctions(db);
  rc = sqlite3_errcode(db);


  /* Load compiled-in extensions */
  for(i=0; rc==SQLITE_OK && i<ArraySize(sqlite3BuiltinExtensions); i++){
    rc = sqlite3BuiltinExtensions[i](db);
  }

  /* Load automatic extensions - extensions that have been registered
  ** using the sqlite3_automatic_extension() API.
  */
  if( rc==SQLITE_OK ){
    sqlite3AutoLoadExtensions(db);
    rc = sqlite3_errcode(db);
    if( rc!=SQLITE_OK ){
      goto opendb_out;
    }
  }

#ifdef SQLITE_ENABLE_INTERNAL_FUNCTIONS
  /* Testing use only!!! The -DSQLITE_ENABLE_INTERNAL_FUNCTIONS=1 compile-time
  ** option gives access to internal functions by default.  
  ** Testing use only!!! */
  db->mDbFlags |= DBFLAG_InternalFunc;
#endif

  /* -DSQLITE_DEFAULT_LOCKING_MODE=1 makes EXCLUSIVE the default locking
  ** mode.  -DSQLITE_DEFAULT_LOCKING_MODE=0 make NORMAL the default locking
  ** mode.  Doing nothing at all also makes NORMAL the default.
  */
#ifdef SQLITE_DEFAULT_LOCKING_MODE
  db->dfltLockMode = SQLITE_DEFAULT_LOCKING_MODE;
  sqlite3PagerLockingMode(sqlite3BtreePager(db->aDb[0].pBt),
                          SQLITE_DEFAULT_LOCKING_MODE);
#endif

  if( rc ) sqlite3Error(db, rc);

  /* Enable the lookaside-malloc subsystem */
  setupLookaside(db, 0, sqlite3GlobalConfig.szLookaside,
                        sqlite3GlobalConfig.nLookaside);

  sqlite3_wal_autocheckpoint(db, SQLITE_DEFAULT_WAL_AUTOCHECKPOINT);

opendb_out:
  if( db ){
    assert( db->mutex!=0 || isThreadsafe==0
           || sqlite3GlobalConfig.bFullMutex==0 );
    sqlite3_mutex_leave(db->mutex);
  }
  rc = sqlite3_errcode(db);
  assert( db!=0 || (rc&0xff)==SQLITE_NOMEM );
  if( (rc&0xff)==SQLITE_NOMEM ){
    sqlite3_close(db);
    db = 0;
  }else if( rc!=SQLITE_OK ){
    db->eOpenState = SQLITE_STATE_SICK;
  }
  *ppDb = db;
#ifdef SQLITE_ENABLE_SQLLOG
  if( sqlite3GlobalConfig.xSqllog ){
    /* Opening a db handle. Fourth parameter is passed 0. */
    void *pArg = sqlite3GlobalConfig.pSqllogArg;
    sqlite3GlobalConfig.xSqllog(pArg, db, zFilename, 0);
  }
#endif
  sqlite3_free_filename(zOpen);
  return rc;
}


/*
** Open a new database handle.
*/
int sqlite3_open(
  const char *zFilename, 
  sqlite3 **ppDb 
){
  return openDatabase(zFilename, ppDb,
                      SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, 0);
}
int sqlite3_open_v2(
  const char *filename,   /* Database filename (UTF-8) */
  sqlite3 **ppDb,         /* OUT: SQLite db handle */
  int flags,              /* Flags */
  const char *zVfs        /* Name of VFS module to use */
){
  return openDatabase(filename, ppDb, (unsigned int)flags, zVfs);
}

#ifndef SQLITE_OMIT_UTF16
/*
** Open a new database handle.
*/
int sqlite3_open16(
  const void *zFilename, 
  sqlite3 **ppDb
){
  char const *zFilename8;   /* zFilename encoded in UTF-8 instead of UTF-16 */
  sqlite3_value *pVal;
  int rc;

#ifdef SQLITE_ENABLE_API_ARMOR
  if( ppDb==0 ) return SQLITE_MISUSE_BKPT;
#endif
  *ppDb = 0;
#ifndef SQLITE_OMIT_AUTOINIT
  rc = sqlite3_initialize();
  if( rc ) return rc;
#endif
  if( zFilename==0 ) zFilename = "\000\000";
  pVal = sqlite3ValueNew(0);
  sqlite3ValueSetStr(pVal, -1, zFilename, SQLITE_UTF16NATIVE, SQLITE_STATIC);
  zFilename8 = sqlite3ValueText(pVal, SQLITE_UTF8);
  if( zFilename8 ){
    rc = openDatabase(zFilename8, ppDb,
                      SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, 0);
    assert( *ppDb || rc==SQLITE_NOMEM );
    if( rc==SQLITE_OK && !DbHasProperty(*ppDb, 0, DB_SchemaLoaded) ){
      SCHEMA_ENC(*ppDb) = ENC(*ppDb) = SQLITE_UTF16NATIVE;
    }
  }else{
    rc = SQLITE_NOMEM_BKPT;
  }
  sqlite3ValueFree(pVal);

  return rc & 0xff;
}
#endif /* SQLITE_OMIT_UTF16 */

/*
** Register a new collation sequence with the database handle db.
*/
int sqlite3_create_collation(
  sqlite3* db, 
  const char *zName, 
  int enc, 
  void* pCtx,
  int(*xCompare)(void*,int,const void*,int,const void*)
){
  return sqlite3_create_collation_v2(db, zName, enc, pCtx, xCompare, 0);
}

/*
** Register a new collation sequence with the database handle db.
*/
int sqlite3_create_collation_v2(
  sqlite3* db, 
  const char *zName, 
  int enc, 
  void* pCtx,
  int(*xCompare)(void*,int,const void*,int,const void*),
  void(*xDel)(void*)
){
  int rc;

#ifdef SQLITE_ENABLE_API_ARMOR
  if( !sqlite3SafetyCheckOk(db) || zName==0 ) return SQLITE_MISUSE_BKPT;
#endif
  sqlite3_mutex_enter(db->mutex);
  assert( !db->mallocFailed );
  rc = createCollation(db, zName, (u8)enc, pCtx, xCompare, xDel);
  rc = sqlite3ApiExit(db, rc);
  sqlite3_mutex_leave(db->mutex);
  return rc;
}

#ifndef SQLITE_OMIT_UTF16
/*
** Register a new collation sequence with the database handle db.
*/
int sqlite3_create_collation16(
  sqlite3* db, 
  const void *zName,
  int enc, 
  void* pCtx,
  int(*xCompare)(void*,int,const void*,int,const void*)
){
  int rc = SQLITE_OK;
  char *zName8;

#ifdef SQLITE_ENABLE_API_ARMOR
  if( !sqlite3SafetyCheckOk(db) || zName==0 ) return SQLITE_MISUSE_BKPT;
#endif
  sqlite3_mutex_enter(db->mutex);
  assert( !db->mallocFailed );
  zName8 = sqlite3Utf16to8(db, zName, -1, SQLITE_UTF16NATIVE);
  if( zName8 ){
    rc = createCollation(db, zName8, (u8)enc, pCtx, xCompare, 0);
    sqlite3DbFree(db, zName8);
  }
  rc = sqlite3ApiExit(db, rc);
  sqlite3_mutex_leave(db->mutex);
  return rc;
}
#endif /* SQLITE_OMIT_UTF16 */

/*
** Register a collation sequence factory callback with the database handle
** db. Replace any previously installed collation sequence factory.
*/
int sqlite3_collation_needed(
  sqlite3 *db, 
  void *pCollNeededArg, 
  void(*xCollNeeded)(void*,sqlite3*,int eTextRep,const char*)
){
#ifdef SQLITE_ENABLE_API_ARMOR
  if( !sqlite3SafetyCheckOk(db) ) return SQLITE_MISUSE_BKPT;
#endif
  sqlite3_mutex_enter(db->mutex);
  db->xCollNeeded = xCollNeeded;
  db->xCollNeeded16 = 0;
  db->pCollNeededArg = pCollNeededArg;
  sqlite3_mutex_leave(db->mutex);
  return SQLITE_OK;
}

#ifndef SQLITE_OMIT_UTF16
/*
** Register a collation sequence factory callback with the database handle
** db. Replace any previously installed collation sequence factory.
*/
int sqlite3_collation_needed16(
  sqlite3 *db, 
  void *pCollNeededArg, 
  void(*xCollNeeded16)(void*,sqlite3*,int eTextRep,const void*)
){
#ifdef SQLITE_ENABLE_API_ARMOR
  if( !sqlite3SafetyCheckOk(db) ) return SQLITE_MISUSE_BKPT;
#endif
  sqlite3_mutex_enter(db->mutex);
  db->xCollNeeded = 0;
  db->xCollNeeded16 = xCollNeeded16;
  db->pCollNeededArg = pCollNeededArg;
  sqlite3_mutex_leave(db->mutex);
  return SQLITE_OK;
}
#endif /* SQLITE_OMIT_UTF16 */

#ifndef SQLITE_OMIT_DEPRECATED
/*
** This function is now an anachronism. It used to be used to recover from a
** malloc() failure, but SQLite now does this automatically.
*/
int sqlite3_global_recover(void){
  return SQLITE_OK;
}
#endif

/*
** Test to see whether or not the database connection is in autocommit
** mode.  Return TRUE if it is and FALSE if not.  Autocommit mode is on
** by default.  Autocommit is disabled by a BEGIN statement and reenabled
** by the next COMMIT or ROLLBACK.
*/
int sqlite3_get_autocommit(sqlite3 *db){
#ifdef SQLITE_ENABLE_API_ARMOR
  if( !sqlite3SafetyCheckOk(db) ){
    (void)SQLITE_MISUSE_BKPT;
    return 0;
  }
#endif
  return db->autoCommit;
}

/*
** The following routines are substitutes for constants SQLITE_CORRUPT,
** SQLITE_MISUSE, SQLITE_CANTOPEN, SQLITE_NOMEM and possibly other error
** constants.  They serve two purposes:
**
**   1.  Serve as a convenient place to set a breakpoint in a debugger
**       to detect when version error conditions occurs.
**
**   2.  Invoke sqlite3_log() to provide the source code location where
**       a low-level error is first detected.
*/
int sqlite3ReportError(int iErr, int lineno, const char *zType){
  sqlite3_log(iErr, "%s at line %d of [%.10s]",
              zType, lineno, 20+sqlite3_sourceid());
  return iErr;
}
int sqlite3CorruptError(int lineno){
  testcase( sqlite3GlobalConfig.xLog!=0 );
  return sqlite3ReportError(SQLITE_CORRUPT, lineno, "database corruption");
}
int sqlite3MisuseError(int lineno){
  testcase( sqlite3GlobalConfig.xLog!=0 );
  return sqlite3ReportError(SQLITE_MISUSE, lineno, "misuse");
}
int sqlite3CantopenError(int lineno){
  testcase( sqlite3GlobalConfig.xLog!=0 );
  return sqlite3ReportError(SQLITE_CANTOPEN, lineno, "cannot open file");
}
#if defined(SQLITE_DEBUG) || defined(SQLITE_ENABLE_CORRUPT_PGNO)
int sqlite3CorruptPgnoError(int lineno, Pgno pgno){
  char zMsg[100];
  sqlite3_snprintf(sizeof(zMsg), zMsg, "database corruption page %d", pgno);
  testcase( sqlite3GlobalConfig.xLog!=0 );
  return sqlite3ReportError(SQLITE_CORRUPT, lineno, zMsg);
}
#endif
#ifdef SQLITE_DEBUG
int sqlite3NomemError(int lineno){
  testcase( sqlite3GlobalConfig.xLog!=0 );
  return sqlite3ReportError(SQLITE_NOMEM, lineno, "OOM");
}
int sqlite3IoerrnomemError(int lineno){
  testcase( sqlite3GlobalConfig.xLog!=0 );
  return sqlite3ReportError(SQLITE_IOERR_NOMEM, lineno, "I/O OOM error");
}
#endif

#ifndef SQLITE_OMIT_DEPRECATED
/*
** This is a convenience routine that makes sure that all thread-specific
** data for this thread has been deallocated.
**
** SQLite no longer uses thread-specific data so this routine is now a
** no-op.  It is retained for historical compatibility.
*/
void sqlite3_thread_cleanup(void){
}
#endif

/*
** Return meta information about a specific column of a database table.
** See comment in sqlite3.h (sqlite.h.in) for details.
*/
int sqlite3_table_column_metadata(
  sqlite3 *db,                /* Connection handle */
  const char *zDbName,        /* Database name or NULL */
  const char *zTableName,     /* Table name */
  const char *zColumnName,    /* Column name */
  char const **pzDataType,    /* OUTPUT: Declared data type */
  char const **pzCollSeq,     /* OUTPUT: Collation sequence name */
  int *pNotNull,              /* OUTPUT: True if NOT NULL constraint exists */
  int *pPrimaryKey,           /* OUTPUT: True if column part of PK */
  int *pAutoinc               /* OUTPUT: True if column is auto-increment */
){
  int rc;
  char *zErrMsg = 0;
  Table *pTab = 0;
  Column *pCol = 0;
  int iCol = 0;
  char const *zDataType = 0;
  char const *zCollSeq = 0;
  int notnull = 0;
  int primarykey = 0;
  int autoinc = 0;


#ifdef SQLITE_ENABLE_API_ARMOR
  if( !sqlite3SafetyCheckOk(db) || zTableName==0 ){
    return SQLITE_MISUSE_BKPT;
  }
#endif

  /* Ensure the database schema has been loaded */
  sqlite3_mutex_enter(db->mutex);
  sqlite3BtreeEnterAll(db);
  rc = sqlite3Init(db, &zErrMsg);
  if( SQLITE_OK!=rc ){
    goto error_out;
  }

  /* Locate the table in question */
  pTab = sqlite3FindTable(db, zTableName, zDbName);
  if( !pTab || IsView(pTab) ){
    pTab = 0;
    goto error_out;
  }

  /* Find the column for which info is requested */
  if( zColumnName==0 ){
    /* Query for existance of table only */
  }else{
    for(iCol=0; iCol<pTab->nCol; iCol++){
      pCol = &pTab->aCol[iCol];
      if( 0==sqlite3StrICmp(pCol->zCnName, zColumnName) ){
        break;
      }
    }
    if( iCol==pTab->nCol ){
      if( HasRowid(pTab) && sqlite3IsRowid(zColumnName) ){
        iCol = pTab->iPKey;
        pCol = iCol>=0 ? &pTab->aCol[iCol] : 0;
      }else{
        pTab = 0;
        goto error_out;
      }
    }
  }

  /* The following block stores the meta information that will be returned
  ** to the caller in local variables zDataType, zCollSeq, notnull, primarykey
  ** and autoinc. At this point there are two possibilities:
  ** 
  **     1. The specified column name was rowid", "oid" or "_rowid_" 
  **        and there is no explicitly declared IPK column. 
  **
  **     2. The table is not a view and the column name identified an 
  **        explicitly declared column. Copy meta information from *pCol.
  */ 
  if( pCol ){
    zDataType = sqlite3ColumnType(pCol,0);
    zCollSeq = sqlite3ColumnColl(pCol);
    notnull = pCol->notNull!=0;
    primarykey  = (pCol->colFlags & COLFLAG_PRIMKEY)!=0;
    autoinc = pTab->iPKey==iCol && (pTab->tabFlags & TF_Autoincrement)!=0;
  }else{
    zDataType = "INTEGER";
    primarykey = 1;
  }
  if( !zCollSeq ){
    zCollSeq = sqlite3StrBINARY;
  }

error_out:
  sqlite3BtreeLeaveAll(db);

  /* Whether the function call succeeded or failed, set the output parameters
  ** to whatever their local counterparts contain. If an error did occur,
  ** this has the effect of zeroing all output parameters.
  */
  if( pzDataType ) *pzDataType = zDataType;
  if( pzCollSeq ) *pzCollSeq = zCollSeq;
  if( pNotNull ) *pNotNull = notnull;
  if( pPrimaryKey ) *pPrimaryKey = primarykey;
  if( pAutoinc ) *pAutoinc = autoinc;

  if( SQLITE_OK==rc && !pTab ){
    sqlite3DbFree(db, zErrMsg);
    zErrMsg = sqlite3MPrintf(db, "no such table column: %s.%s", zTableName,
        zColumnName);
    rc = SQLITE_ERROR;
  }
  sqlite3ErrorWithMsg(db, rc, (zErrMsg?"%s":0), zErrMsg);
  sqlite3DbFree(db, zErrMsg);
  rc = sqlite3ApiExit(db, rc);
  sqlite3_mutex_leave(db->mutex);
  return rc;
}

/*
** Sleep for a little while.  Return the amount of time slept.
*/
int sqlite3_sleep(int ms){
  sqlite3_vfs *pVfs;
  int rc;
  pVfs = sqlite3_vfs_find(0);
  if( pVfs==0 ) return 0;

  /* This function works in milliseconds, but the underlying OsSleep() 
  ** API uses microseconds. Hence the 1000's.
  */
  rc = (sqlite3OsSleep(pVfs, ms<0 ? 0 : 1000*ms)/1000);
  return rc;
}

/*
** Enable or disable the extended result codes.
*/
int sqlite3_extended_result_codes(sqlite3 *db, int onoff){
#ifdef SQLITE_ENABLE_API_ARMOR
  if( !sqlite3SafetyCheckOk(db) ) return SQLITE_MISUSE_BKPT;
#endif
  sqlite3_mutex_enter(db->mutex);
  db->errMask = onoff ? 0xffffffff : 0xff;
  sqlite3_mutex_leave(db->mutex);
  return SQLITE_OK;
}

/*
** Invoke the xFileControl method on a particular database.
*/
int sqlite3_file_control(sqlite3 *db, const char *zDbName, int op, void *pArg){
  int rc = SQLITE_ERROR;
  Btree *pBtree;

#ifdef SQLITE_ENABLE_API_ARMOR
  if( !sqlite3SafetyCheckOk(db) ) return SQLITE_MISUSE_BKPT;
#endif
  sqlite3_mutex_enter(db->mutex);
  pBtree = sqlite3DbNameToBtree(db, zDbName);
  if( pBtree ){
    Pager *pPager;
    sqlite3_file *fd;
    sqlite3BtreeEnter(pBtree);
    pPager = sqlite3BtreePager(pBtree);
    assert( pPager!=0 );
    fd = sqlite3PagerFile(pPager);
    assert( fd!=0 );
    if( op==SQLITE_FCNTL_FILE_POINTER ){
      *(sqlite3_file**)pArg = fd;
      rc = SQLITE_OK;
    }else if( op==SQLITE_FCNTL_VFS_POINTER ){
      *(sqlite3_vfs**)pArg = sqlite3PagerVfs(pPager);
      rc = SQLITE_OK;
    }else if( op==SQLITE_FCNTL_JOURNAL_POINTER ){
      *(sqlite3_file**)pArg = sqlite3PagerJrnlFile(pPager);
      rc = SQLITE_OK;
    }else if( op==SQLITE_FCNTL_DATA_VERSION ){
      *(unsigned int*)pArg = sqlite3PagerDataVersion(pPager);
      rc = SQLITE_OK;
    }else if( op==SQLITE_FCNTL_RESERVE_BYTES ){
      int iNew = *(int*)pArg;
      *(int*)pArg = sqlite3BtreeGetRequestedReserve(pBtree);
      if( iNew>=0 && iNew<=255 ){
        sqlite3BtreeSetPageSize(pBtree, 0, iNew, 0);
      }
      rc = SQLITE_OK;
    }else if( op==SQLITE_FCNTL_RESET_CACHE ){
      sqlite3BtreeClearCache(pBtree);
      rc = SQLITE_OK;
    }else{
      int nSave = db->busyHandler.nBusy;
      rc = sqlite3OsFileControl(fd, op, pArg);
      db->busyHandler.nBusy = nSave;
    }
    sqlite3BtreeLeave(pBtree);
  }
  sqlite3_mutex_leave(db->mutex);
  return rc;
}

/*
** Interface to the testing logic.
*/
int sqlite3_test_control(int op, ...){
  int rc = 0;
#ifdef SQLITE_UNTESTABLE
  UNUSED_PARAMETER(op);
#else
  va_list ap;
  va_start(ap, op);
  switch( op ){

    /*
    ** Save the current state of the PRNG.
    */
    case SQLITE_TESTCTRL_PRNG_SAVE: {
      sqlite3PrngSaveState();
      break;
    }

    /*
    ** Restore the state of the PRNG to the last state saved using
    ** PRNG_SAVE.  If PRNG_SAVE has never before been called, then
    ** this verb acts like PRNG_RESET.
    */
    case SQLITE_TESTCTRL_PRNG_RESTORE: {
      sqlite3PrngRestoreState();
      break;
    }

    /*  sqlite3_test_control(SQLITE_TESTCTRL_PRNG_SEED, int x, sqlite3 *db);
    **
    ** Control the seed for the pseudo-random number generator (PRNG) that
    ** is built into SQLite.  Cases:
    **
    **    x!=0 && db!=0       Seed the PRNG to the current value of the
    **                        schema cookie in the main database for db, or
    **                        x if the schema cookie is zero.  This case
    **                        is convenient to use with database fuzzers
    **                        as it allows the fuzzer some control over the
    **                        the PRNG seed.
    **
    **    x!=0 && db==0       Seed the PRNG to the value of x.
    **
    **    x==0 && db==0       Revert to default behavior of using the
    **                        xRandomness method on the primary VFS.
    **
    ** This test-control also resets the PRNG so that the new seed will
    ** be used for the next call to sqlite3_randomness().
    */
#ifndef SQLITE_OMIT_WSD
    case SQLITE_TESTCTRL_PRNG_SEED: {
      int x = va_arg(ap, int);
      int y;
      sqlite3 *db = va_arg(ap, sqlite3*);
      assert( db==0 || db->aDb[0].pSchema!=0 );
      if( db && (y = db->aDb[0].pSchema->schema_cookie)!=0 ){ x = y; }
      sqlite3Config.iPrngSeed = x;
      sqlite3_randomness(0,0);
      break;
    }
#endif

    /*
    **  sqlite3_test_control(BITVEC_TEST, size, program)
    **
    ** Run a test against a Bitvec object of size.  The program argument
    ** is an array of integers that defines the test.  Return -1 on a
    ** memory allocation error, 0 on success, or non-zero for an error.
    ** See the sqlite3BitvecBuiltinTest() for additional information.
    */
    case SQLITE_TESTCTRL_BITVEC_TEST: {
      int sz = va_arg(ap, int);
      int *aProg = va_arg(ap, int*);
      rc = sqlite3BitvecBuiltinTest(sz, aProg);
      break;
    }

    /*
    **  sqlite3_test_control(FAULT_INSTALL, xCallback)
    **
    ** Arrange to invoke xCallback() whenever sqlite3FaultSim() is called,
    ** if xCallback is not NULL.
    **
    ** As a test of the fault simulator mechanism itself, sqlite3FaultSim(0)
    ** is called immediately after installing the new callback and the return
    ** value from sqlite3FaultSim(0) becomes the return from
    ** sqlite3_test_control().
    */
    case SQLITE_TESTCTRL_FAULT_INSTALL: {
      /* A bug in MSVC prevents it from understanding pointers to functions
      ** types in the second argument to va_arg().  Work around the problem
      ** using a typedef.
      ** http://support.microsoft.com/kb/47961  <-- dead hyperlink
      ** Search at http://web.archive.org/ to find the 2015-03-16 archive
      ** of the link above to see the original text.
      ** sqlite3GlobalConfig.xTestCallback = va_arg(ap, int(*)(int));
      */
      typedef int(*sqlite3FaultFuncType)(int);
      sqlite3GlobalConfig.xTestCallback = va_arg(ap, sqlite3FaultFuncType);
      rc = sqlite3FaultSim(0);
      break;
    }

    /*
    **  sqlite3_test_control(BENIGN_MALLOC_HOOKS, xBegin, xEnd)
    **
    ** Register hooks to call to indicate which malloc() failures 
    ** are benign.
    */
    case SQLITE_TESTCTRL_BENIGN_MALLOC_HOOKS: {
      typedef void (*void_function)(void);
      void_function xBenignBegin;
      void_function xBenignEnd;
      xBenignBegin = va_arg(ap, void_function);
      xBenignEnd = va_arg(ap, void_function);
      sqlite3BenignMallocHooks(xBenignBegin, xBenignEnd);
      break;
    }

    /*
    **  sqlite3_test_control(SQLITE_TESTCTRL_PENDING_BYTE, unsigned int X)
    **
    ** Set the PENDING byte to the value in the argument, if X>0.
    ** Make no changes if X==0.  Return the value of the pending byte
    ** as it existing before this routine was called.
    **
    ** IMPORTANT:  Changing the PENDING byte from 0x40000000 results in
    ** an incompatible database file format.  Changing the PENDING byte
    ** while any database connection is open results in undefined and
    ** deleterious behavior.
    */
    case SQLITE_TESTCTRL_PENDING_BYTE: {
      rc = PENDING_BYTE;
#ifndef SQLITE_OMIT_WSD
      {
        unsigned int newVal = va_arg(ap, unsigned int);
        if( newVal ) sqlite3PendingByte = newVal;
      }
#endif
      break;
    }

    /*
    **  sqlite3_test_control(SQLITE_TESTCTRL_ASSERT, int X)
    **
    ** This action provides a run-time test to see whether or not
    ** assert() was enabled at compile-time.  If X is true and assert()
    ** is enabled, then the return value is true.  If X is true and
    ** assert() is disabled, then the return value is zero.  If X is
    ** false and assert() is enabled, then the assertion fires and the
    ** process aborts.  If X is false and assert() is disabled, then the
    ** return value is zero.
    */
    case SQLITE_TESTCTRL_ASSERT: {
      volatile int x = 0;
      assert( /*side-effects-ok*/ (x = va_arg(ap,int))!=0 );
      rc = x;
#if defined(SQLITE_DEBUG)
      /* Invoke these debugging routines so that the compiler does not
      ** issue "defined but not used" warnings. */
      if( x==9999 ){
        sqlite3ShowExpr(0);
        sqlite3ShowExpr(0);
        sqlite3ShowExprList(0);
        sqlite3ShowIdList(0);
        sqlite3ShowSrcList(0);
        sqlite3ShowWith(0);
        sqlite3ShowUpsert(0);
        sqlite3ShowTriggerStep(0);
        sqlite3ShowTriggerStepList(0);
        sqlite3ShowTrigger(0);
        sqlite3ShowTriggerList(0);
#ifndef SQLITE_OMIT_WINDOWFUNC
        sqlite3ShowWindow(0);
        sqlite3ShowWinFunc(0);
#endif
        sqlite3ShowSelect(0);
      }
#endif
      break;
    }


    /*
    **  sqlite3_test_control(SQLITE_TESTCTRL_ALWAYS, int X)
    **
    ** This action provides a run-time test to see how the ALWAYS and
    ** NEVER macros were defined at compile-time.
    **
    ** The return value is ALWAYS(X) if X is true, or 0 if X is false.
    **
    ** The recommended test is X==2.  If the return value is 2, that means
    ** ALWAYS() and NEVER() are both no-op pass-through macros, which is the
    ** default setting.  If the return value is 1, then ALWAYS() is either
    ** hard-coded to true or else it asserts if its argument is false.
    ** The first behavior (hard-coded to true) is the case if
    ** SQLITE_TESTCTRL_ASSERT shows that assert() is disabled and the second
    ** behavior (assert if the argument to ALWAYS() is false) is the case if
    ** SQLITE_TESTCTRL_ASSERT shows that assert() is enabled.
    **
    ** The run-time test procedure might look something like this:
    **
    **    if( sqlite3_test_control(SQLITE_TESTCTRL_ALWAYS, 2)==2 ){
    **      // ALWAYS() and NEVER() are no-op pass-through macros
    **    }else if( sqlite3_test_control(SQLITE_TESTCTRL_ASSERT, 1) ){
    **      // ALWAYS(x) asserts that x is true. NEVER(x) asserts x is false.
    **    }else{
    **      // ALWAYS(x) is a constant 1.  NEVER(x) is a constant 0.
    **    }
    */
    case SQLITE_TESTCTRL_ALWAYS: {
      int x = va_arg(ap,int);
      rc = x ? ALWAYS(x) : 0;
      break;
    }

    /*
    **   sqlite3_test_control(SQLITE_TESTCTRL_BYTEORDER);
    **
    ** The integer returned reveals the byte-order of the computer on which
    ** SQLite is running:
    **
    **       1     big-endian,    determined at run-time
    **      10     little-endian, determined at run-time
    **  432101     big-endian,    determined at compile-time
    **  123410     little-endian, determined at compile-time
    */ 
    case SQLITE_TESTCTRL_BYTEORDER: {
      rc = SQLITE_BYTEORDER*100 + SQLITE_LITTLEENDIAN*10 + SQLITE_BIGENDIAN;
      break;
    }

    /*  sqlite3_test_control(SQLITE_TESTCTRL_OPTIMIZATIONS, sqlite3 *db, int N)
    **
    ** Enable or disable various optimizations for testing purposes.  The 
    ** argument N is a bitmask of optimizations to be disabled.  For normal
    ** operation N should be 0.  The idea is that a test program (like the
    ** SQL Logic Test or SLT test module) can run the same SQL multiple times
    ** with various optimizations disabled to verify that the same answer
    ** is obtained in every case.
    */
    case SQLITE_TESTCTRL_OPTIMIZATIONS: {
      sqlite3 *db = va_arg(ap, sqlite3*);
      db->dbOptFlags = va_arg(ap, u32);
      break;
    }

    /*   sqlite3_test_control(SQLITE_TESTCTRL_LOCALTIME_FAULT, onoff, xAlt);
    **
    ** If parameter onoff is 1, subsequent calls to localtime() fail.
    ** If 2, then invoke xAlt() instead of localtime().  If 0, normal
    ** processing.
    **
    ** xAlt arguments are void pointers, but they really want to be:
    **
    **    int xAlt(const time_t*, struct tm*);
    **
    ** xAlt should write results in to struct tm object of its 2nd argument
    ** and return zero on success, or return non-zero on failure.
    */
    case SQLITE_TESTCTRL_LOCALTIME_FAULT: {
      sqlite3GlobalConfig.bLocaltimeFault = va_arg(ap, int);
      if( sqlite3GlobalConfig.bLocaltimeFault==2 ){
        typedef int(*sqlite3LocaltimeType)(const void*,void*);
        sqlite3GlobalConfig.xAltLocaltime = va_arg(ap, sqlite3LocaltimeType);
      }else{
        sqlite3GlobalConfig.xAltLocaltime = 0;
      }
      break;
    }

    /*   sqlite3_test_control(SQLITE_TESTCTRL_INTERNAL_FUNCTIONS, sqlite3*);
    **
    ** Toggle the ability to use internal functions on or off for
    ** the database connection given in the argument.
    */
    case SQLITE_TESTCTRL_INTERNAL_FUNCTIONS: {
      sqlite3 *db = va_arg(ap, sqlite3*);
      db->mDbFlags ^= DBFLAG_InternalFunc;
      break;
    }

    /*   sqlite3_test_control(SQLITE_TESTCTRL_NEVER_CORRUPT, int);
    **
    ** Set or clear a flag that indicates that the database file is always well-
    ** formed and never corrupt.  This flag is clear by default, indicating that
    ** database files might have arbitrary corruption.  Setting the flag during
    ** testing causes certain assert() statements in the code to be activated
    ** that demonstrat invariants on well-formed database files.
    */
    case SQLITE_TESTCTRL_NEVER_CORRUPT: {
      sqlite3GlobalConfig.neverCorrupt = va_arg(ap, int);
      break;
    }

    /*   sqlite3_test_control(SQLITE_TESTCTRL_EXTRA_SCHEMA_CHECKS, int);
    **
    ** Set or clear a flag that causes SQLite to verify that type, name,
    ** and tbl_name fields of the sqlite_schema table.  This is normally
    ** on, but it is sometimes useful to turn it off for testing.
    **
    ** 2020-07-22:  Disabling EXTRA_SCHEMA_CHECKS also disables the
    ** verification of rootpage numbers when parsing the schema.  This
    ** is useful to make it easier to reach strange internal error states
    ** during testing.  The EXTRA_SCHEMA_CHECKS setting is always enabled
    ** in production.
    */
    case SQLITE_TESTCTRL_EXTRA_SCHEMA_CHECKS: {
      sqlite3GlobalConfig.bExtraSchemaChecks = va_arg(ap, int);
      break;
    }

    /* Set the threshold at which OP_Once counters reset back to zero.
    ** By default this is 0x7ffffffe (over 2 billion), but that value is
    ** too big to test in a reasonable amount of time, so this control is
    ** provided to set a small and easily reachable reset value.
    */
    case SQLITE_TESTCTRL_ONCE_RESET_THRESHOLD: {
      sqlite3GlobalConfig.iOnceResetThreshold = va_arg(ap, int);
      break;
    }

    /*   sqlite3_test_control(SQLITE_TESTCTRL_VDBE_COVERAGE, xCallback, ptr);
    **
    ** Set the VDBE coverage callback function to xCallback with context 
    ** pointer ptr.
    */
    case SQLITE_TESTCTRL_VDBE_COVERAGE: {
#ifdef SQLITE_VDBE_COVERAGE
      typedef void (*branch_callback)(void*,unsigned int,
                                      unsigned char,unsigned char);
      sqlite3GlobalConfig.xVdbeBranch = va_arg(ap,branch_callback);
      sqlite3GlobalConfig.pVdbeBranchArg = va_arg(ap,void*);
#endif
      break;
    }

    /*   sqlite3_test_control(SQLITE_TESTCTRL_SORTER_MMAP, db, nMax); */
    case SQLITE_TESTCTRL_SORTER_MMAP: {
      sqlite3 *db = va_arg(ap, sqlite3*);
      db->nMaxSorterMmap = va_arg(ap, int);
      break;
    }

    /*   sqlite3_test_control(SQLITE_TESTCTRL_ISINIT);
    **
    ** Return SQLITE_OK if SQLite has been initialized and SQLITE_ERROR if
    ** not.
    */
    case SQLITE_TESTCTRL_ISINIT: {
      if( sqlite3GlobalConfig.isInit==0 ) rc = SQLITE_ERROR;
      break;
    }

    /*  sqlite3_test_control(SQLITE_TESTCTRL_IMPOSTER, db, dbName, onOff, tnum);
    **
    ** This test control is used to create imposter tables.  "db" is a pointer
    ** to the database connection.  dbName is the database name (ex: "main" or
    ** "temp") which will receive the imposter.  "onOff" turns imposter mode on
    ** or off.  "tnum" is the root page of the b-tree to which the imposter
    ** table should connect.
    **
    ** Enable imposter mode only when the schema has already been parsed.  Then
    ** run a single CREATE TABLE statement to construct the imposter table in
    ** the parsed schema.  Then turn imposter mode back off again.
    **
    ** If onOff==0 and tnum>0 then reset the schema for all databases, causing
    ** the schema to be reparsed the next time it is needed.  This has the
    ** effect of erasing all imposter tables.
    */
    case SQLITE_TESTCTRL_IMPOSTER: {
      sqlite3 *db = va_arg(ap, sqlite3*);
      int iDb;
      sqlite3_mutex_enter(db->mutex);
      iDb = sqlite3FindDbName(db, va_arg(ap,const char*));
      if( iDb>=0 ){
        db->init.iDb = iDb;
        db->init.busy = db->init.imposterTable = va_arg(ap,int);
        db->init.newTnum = va_arg(ap,int);
        if( db->init.busy==0 && db->init.newTnum>0 ){
          sqlite3ResetAllSchemasOfConnection(db);
        }
      }
      sqlite3_mutex_leave(db->mutex);
      break;
    }

#if defined(YYCOVERAGE)
    /*  sqlite3_test_control(SQLITE_TESTCTRL_PARSER_COVERAGE, FILE *out)
    **
    ** This test control (only available when SQLite is compiled with
    ** -DYYCOVERAGE) writes a report onto "out" that shows all
    ** state/lookahead combinations in the parser state machine
    ** which are never exercised.  If any state is missed, make the
    ** return code SQLITE_ERROR.
    */
    case SQLITE_TESTCTRL_PARSER_COVERAGE: {
      FILE *out = va_arg(ap, FILE*);
      if( sqlite3ParserCoverage(out) ) rc = SQLITE_ERROR;
      break;
    }
#endif /* defined(YYCOVERAGE) */

    /*  sqlite3_test_control(SQLITE_TESTCTRL_RESULT_INTREAL, sqlite3_context*);
    **
    ** This test-control causes the most recent sqlite3_result_int64() value
    ** to be interpreted as a MEM_IntReal instead of as an MEM_Int.  Normally,
    ** MEM_IntReal values only arise during an INSERT operation of integer
    ** values into a REAL column, so they can be challenging to test.  This
    ** test-control enables us to write an intreal() SQL function that can
    ** inject an intreal() value at arbitrary places in an SQL statement,
    ** for testing purposes.
    */
    case SQLITE_TESTCTRL_RESULT_INTREAL: {
      sqlite3_context *pCtx = va_arg(ap, sqlite3_context*);
      sqlite3ResultIntReal(pCtx);
      break;
    }

    /*  sqlite3_test_control(SQLITE_TESTCTRL_SEEK_COUNT,
    **    sqlite3 *db,    // Database connection
    **    u64 *pnSeek     // Write seek count here
    **  );
    **
    ** This test-control queries the seek-counter on the "main" database
    ** file.  The seek-counter is written into *pnSeek and is then reset.
    ** The seek-count is only available if compiled with SQLITE_DEBUG.
    */
    case SQLITE_TESTCTRL_SEEK_COUNT: {
      sqlite3 *db = va_arg(ap, sqlite3*);
      u64 *pn = va_arg(ap, sqlite3_uint64*);
      *pn = sqlite3BtreeSeekCount(db->aDb->pBt);
      (void)db;  /* Silence harmless unused variable warning */
      break;
    }

    /*  sqlite3_test_control(SQLITE_TESTCTRL_TRACEFLAGS, op, ptr)
    **
    **  "ptr" is a pointer to a u32.  
    **
    **   op==0       Store the current sqlite3TreeTrace in *ptr
    **   op==1       Set sqlite3TreeTrace to the value *ptr
    **   op==3       Store the current sqlite3WhereTrace in *ptr
    **   op==3       Set sqlite3WhereTrace to the value *ptr
    */
    case SQLITE_TESTCTRL_TRACEFLAGS: {
       int opTrace = va_arg(ap, int);
       u32 *ptr = va_arg(ap, u32*);
       switch( opTrace ){
         case 0:   *ptr = sqlite3TreeTrace;      break;
         case 1:   sqlite3TreeTrace = *ptr;      break;
         case 2:   *ptr = sqlite3WhereTrace;     break;
         case 3:   sqlite3WhereTrace = *ptr;     break;
       }
       break;
    }

    /* sqlite3_test_control(SQLITE_TESTCTRL_LOGEST,
    **      double fIn,     // Input value
    **      int *pLogEst,   // sqlite3LogEstFromDouble(fIn)
    **      u64 *pInt,      // sqlite3LogEstToInt(*pLogEst)
    **      int *pLogEst2   // sqlite3LogEst(*pInt)
    ** );
    **
    ** Test access for the LogEst conversion routines.
    */
    case SQLITE_TESTCTRL_LOGEST: {
      double rIn = va_arg(ap, double);
      LogEst rLogEst = sqlite3LogEstFromDouble(rIn);
      int *pI1 = va_arg(ap,int*);
      u64 *pU64 = va_arg(ap,u64*);
      int *pI2 = va_arg(ap,int*);
      *pI1 = rLogEst;
      *pU64 = sqlite3LogEstToInt(rLogEst);
      *pI2 = sqlite3LogEst(*pU64);
      break;
    }
 

#if defined(SQLITE_DEBUG) && !defined(SQLITE_OMIT_WSD)
    /* sqlite3_test_control(SQLITE_TESTCTRL_TUNE, id, *piValue)
    **
    ** If "id" is an integer between 1 and SQLITE_NTUNE then set the value
    ** of the id-th tuning parameter to *piValue.  If "id" is between -1
    ** and -SQLITE_NTUNE, then write the current value of the (-id)-th
    ** tuning parameter into *piValue.
    **
    ** Tuning parameters are for use during transient development builds,
    ** to help find the best values for constants in the query planner.
    ** Access tuning parameters using the Tuning(ID) macro.  Set the
    ** parameters in the CLI using ".testctrl tune ID VALUE".
    **
    ** Transient use only.  Tuning parameters should not be used in
    ** checked-in code.
    */
    case SQLITE_TESTCTRL_TUNE: {
      int id = va_arg(ap, int);
      int *piValue = va_arg(ap, int*);
      if( id>0 && id<=SQLITE_NTUNE ){
        Tuning(id) = *piValue;
      }else if( id<0 && id>=-SQLITE_NTUNE ){
        *piValue = Tuning(-id);
      }else{
        rc = SQLITE_NOTFOUND;
      }
      break;
    }
#endif
  }
  va_end(ap);
#endif /* SQLITE_UNTESTABLE */
  return rc;
}

/*
** The Pager stores the Database filename, Journal filename, and WAL filename
** consecutively in memory, in that order.  The database filename is prefixed
** by four zero bytes.  Locate the start of the database filename by searching
** backwards for the first byte following four consecutive zero bytes.
**
** This only works if the filename passed in was obtained from the Pager.
*/
static const char *databaseName(const char *zName){
  while( zName[-1]!=0 || zName[-2]!=0 || zName[-3]!=0 || zName[-4]!=0 ){
    zName--;
  }
  return zName;
}

/*
** Append text z[] to the end of p[].  Return a pointer to the first
** character after then zero terminator on the new text in p[].
*/
static char *appendText(char *p, const char *z){
  size_t n = strlen(z);
  memcpy(p, z, n+1);
  return p+n+1;
}

/*
** Allocate memory to hold names for a database, journal file, WAL file,
** and query parameters.  The pointer returned is valid for use by
** sqlite3_filename_database() and sqlite3_uri_parameter() and related
** functions.
**
** Memory layout must be compatible with that generated by the pager
** and expected by sqlite3_uri_parameter() and databaseName().
*/
const char *sqlite3_create_filename(
  const char *zDatabase,
  const char *zJournal,
  const char *zWal,
  int nParam,
  const char **azParam
){
  sqlite3_int64 nByte;
  int i;
  char *pResult, *p;
  nByte = strlen(zDatabase) + strlen(zJournal) + strlen(zWal) + 10;
  for(i=0; i<nParam*2; i++){
    nByte += strlen(azParam[i])+1;
  }
  pResult = p = sqlite3_malloc64( nByte );
  if( p==0 ) return 0;
  memset(p, 0, 4);
  p += 4;
  p = appendText(p, zDatabase);
  for(i=0; i<nParam*2; i++){
    p = appendText(p, azParam[i]);
  }
  *(p++) = 0;
  p = appendText(p, zJournal);
  p = appendText(p, zWal);
  *(p++) = 0;
  *(p++) = 0;
  assert( (sqlite3_int64)(p - pResult)==nByte );
  return pResult + 4;
}

/*
** Free memory obtained from sqlite3_create_filename().  It is a severe
** error to call this routine with any parameter other than a pointer
** previously obtained from sqlite3_create_filename() or a NULL pointer.
*/
void sqlite3_free_filename(const char *p){
  if( p==0 ) return;
  p = databaseName(p);
  sqlite3_free((char*)p - 4);
}


/*
** This is a utility routine, useful to VFS implementations, that checks
** to see if a database file was a URI that contained a specific query 
** parameter, and if so obtains the value of the query parameter.
**
** The zFilename argument is the filename pointer passed into the xOpen()
** method of a VFS implementation.  The zParam argument is the name of the
** query parameter we seek.  This routine returns the value of the zParam
** parameter if it exists.  If the parameter does not exist, this routine
** returns a NULL pointer.
*/
const char *sqlite3_uri_parameter(const char *zFilename, const char *zParam){
  if( zFilename==0 || zParam==0 ) return 0;
  zFilename = databaseName(zFilename);
  return uriParameter(zFilename, zParam);
}

/*
** Return a pointer to the name of Nth query parameter of the filename.
*/
const char *sqlite3_uri_key(const char *zFilename, int N){
  if( zFilename==0 || N<0 ) return 0;
  zFilename = databaseName(zFilename);
  zFilename += sqlite3Strlen30(zFilename) + 1;
  while( ALWAYS(zFilename) && zFilename[0] && (N--)>0 ){
    zFilename += sqlite3Strlen30(zFilename) + 1;
    zFilename += sqlite3Strlen30(zFilename) + 1;
  }
  return zFilename[0] ? zFilename : 0;
}

/*
** Return a boolean value for a query parameter.
*/
int sqlite3_uri_boolean(const char *zFilename, const char *zParam, int bDflt){
  const char *z = sqlite3_uri_parameter(zFilename, zParam);
  bDflt = bDflt!=0;
  return z ? sqlite3GetBoolean(z, bDflt) : bDflt;
}

/*
** Return a 64-bit integer value for a query parameter.
*/
sqlite3_int64 sqlite3_uri_int64(
  const char *zFilename,    /* Filename as passed to xOpen */
  const char *zParam,       /* URI parameter sought */
  sqlite3_int64 bDflt       /* return if parameter is missing */
){
  const char *z = sqlite3_uri_parameter(zFilename, zParam);
  sqlite3_int64 v;
  if( z && sqlite3DecOrHexToI64(z, &v)==0 ){
    bDflt = v;
  }
  return bDflt;
}

/*
** Translate a filename that was handed to a VFS routine into the corresponding
** database, journal, or WAL file.
**
** It is an error to pass this routine a filename string that was not
** passed into the VFS from the SQLite core.  Doing so is similar to
** passing free() a pointer that was not obtained from malloc() - it is
** an error that we cannot easily detect but that will likely cause memory
** corruption.
*/
const char *sqlite3_filename_database(const char *zFilename){
  if( zFilename==0 ) return 0;
  return databaseName(zFilename);
}
const char *sqlite3_filename_journal(const char *zFilename){
  if( zFilename==0 ) return 0;
  zFilename = databaseName(zFilename);
  zFilename += sqlite3Strlen30(zFilename) + 1;
  while( ALWAYS(zFilename) && zFilename[0] ){
    zFilename += sqlite3Strlen30(zFilename) + 1;
    zFilename += sqlite3Strlen30(zFilename) + 1;
  }
  return zFilename + 1;
}
const char *sqlite3_filename_wal(const char *zFilename){
#ifdef SQLITE_OMIT_WAL
  return 0;
#else
  zFilename = sqlite3_filename_journal(zFilename);
  if( zFilename ) zFilename += sqlite3Strlen30(zFilename) + 1;
  return zFilename;
#endif
}

/*
** Return the Btree pointer identified by zDbName.  Return NULL if not found.
*/
Btree *sqlite3DbNameToBtree(sqlite3 *db, const char *zDbName){
  int iDb = zDbName ? sqlite3FindDbName(db, zDbName) : 0;
  return iDb<0 ? 0 : db->aDb[iDb].pBt;
}

/*
** Return the name of the N-th database schema.  Return NULL if N is out
** of range.
*/
const char *sqlite3_db_name(sqlite3 *db, int N){
#ifdef SQLITE_ENABLE_API_ARMOR
  if( !sqlite3SafetyCheckOk(db) ){
    (void)SQLITE_MISUSE_BKPT;
    return 0;
  }
#endif
  if( N<0 || N>=db->nDb ){
    return 0;
  }else{
    return db->aDb[N].zDbSName;
  }
}

/*
** Return the filename of the database associated with a database
** connection.
*/
const char *sqlite3_db_filename(sqlite3 *db, const char *zDbName){
  Btree *pBt;
#ifdef SQLITE_ENABLE_API_ARMOR
  if( !sqlite3SafetyCheckOk(db) ){
    (void)SQLITE_MISUSE_BKPT;
    return 0;
  }
#endif
  pBt = sqlite3DbNameToBtree(db, zDbName);
  return pBt ? sqlite3BtreeGetFilename(pBt) : 0;
}

/*
** Return 1 if database is read-only or 0 if read/write.  Return -1 if
** no such database exists.
*/
int sqlite3_db_readonly(sqlite3 *db, const char *zDbName){
  Btree *pBt;
#ifdef SQLITE_ENABLE_API_ARMOR
  if( !sqlite3SafetyCheckOk(db) ){
    (void)SQLITE_MISUSE_BKPT;
    return -1;
  }
#endif
  pBt = sqlite3DbNameToBtree(db, zDbName);
  return pBt ? sqlite3BtreeIsReadonly(pBt) : -1;
}

#ifdef SQLITE_ENABLE_SNAPSHOT
/*
** Obtain a snapshot handle for the snapshot of database zDb currently 
** being read by handle db.
*/
int sqlite3_snapshot_get(
  sqlite3 *db, 
  const char *zDb,
  sqlite3_snapshot **ppSnapshot
){
  int rc = SQLITE_ERROR;
#ifndef SQLITE_OMIT_WAL

#ifdef SQLITE_ENABLE_API_ARMOR
  if( !sqlite3SafetyCheckOk(db) ){
    return SQLITE_MISUSE_BKPT;
  }
#endif
  sqlite3_mutex_enter(db->mutex);

  if( db->autoCommit==0 ){
    int iDb = sqlite3FindDbName(db, zDb);
    if( iDb==0 || iDb>1 ){
      Btree *pBt = db->aDb[iDb].pBt;
      if( SQLITE_TXN_WRITE!=sqlite3BtreeTxnState(pBt) ){
        rc = sqlite3BtreeBeginTrans(pBt, 0, 0);
        if( rc==SQLITE_OK ){
          rc = sqlite3PagerSnapshotGet(sqlite3BtreePager(pBt), ppSnapshot);
        }
      }
    }
  }

  sqlite3_mutex_leave(db->mutex);
#endif   /* SQLITE_OMIT_WAL */
  return rc;
}

/*
** Open a read-transaction on the snapshot idendified by pSnapshot.
*/
int sqlite3_snapshot_open(
  sqlite3 *db, 
  const char *zDb, 
  sqlite3_snapshot *pSnapshot
){
  int rc = SQLITE_ERROR;
#ifndef SQLITE_OMIT_WAL

#ifdef SQLITE_ENABLE_API_ARMOR
  if( !sqlite3SafetyCheckOk(db) ){
    return SQLITE_MISUSE_BKPT;
  }
#endif
  sqlite3_mutex_enter(db->mutex);
  if( db->autoCommit==0 ){
    int iDb;
    iDb = sqlite3FindDbName(db, zDb);
    if( iDb==0 || iDb>1 ){
      Btree *pBt = db->aDb[iDb].pBt;
      if( sqlite3BtreeTxnState(pBt)!=SQLITE_TXN_WRITE ){
        Pager *pPager = sqlite3BtreePager(pBt);
        int bUnlock = 0;
        if( sqlite3BtreeTxnState(pBt)!=SQLITE_TXN_NONE ){
          if( db->nVdbeActive==0 ){
            rc = sqlite3PagerSnapshotCheck(pPager, pSnapshot);
            if( rc==SQLITE_OK ){
              bUnlock = 1;
              rc = sqlite3BtreeCommit(pBt);
            }
          }
        }else{
          rc = SQLITE_OK;
        }
        if( rc==SQLITE_OK ){
          rc = sqlite3PagerSnapshotOpen(pPager, pSnapshot);
        }
        if( rc==SQLITE_OK ){
          rc = sqlite3BtreeBeginTrans(pBt, 0, 0);
          sqlite3PagerSnapshotOpen(pPager, 0);
        }
        if( bUnlock ){
          sqlite3PagerSnapshotUnlock(pPager);
        }
      }
    }
  }

  sqlite3_mutex_leave(db->mutex);
#endif   /* SQLITE_OMIT_WAL */
  return rc;
}

/*
** Recover as many snapshots as possible from the wal file associated with
** schema zDb of database db.
*/
int sqlite3_snapshot_recover(sqlite3 *db, const char *zDb){
  int rc = SQLITE_ERROR;
#ifndef SQLITE_OMIT_WAL
  int iDb;

#ifdef SQLITE_ENABLE_API_ARMOR
  if( !sqlite3SafetyCheckOk(db) ){
    return SQLITE_MISUSE_BKPT;
  }
#endif

  sqlite3_mutex_enter(db->mutex);
  iDb = sqlite3FindDbName(db, zDb);
  if( iDb==0 || iDb>1 ){
    Btree *pBt = db->aDb[iDb].pBt;
    if( SQLITE_TXN_NONE==sqlite3BtreeTxnState(pBt) ){
      rc = sqlite3BtreeBeginTrans(pBt, 0, 0);
      if( rc==SQLITE_OK ){
        rc = sqlite3PagerSnapshotRecover(sqlite3BtreePager(pBt));
        sqlite3BtreeCommit(pBt);
      }
    }
  }
  sqlite3_mutex_leave(db->mutex);
#endif   /* SQLITE_OMIT_WAL */
  return rc;
}

/*
** Free a snapshot handle obtained from sqlite3_snapshot_get().
*/
void sqlite3_snapshot_free(sqlite3_snapshot *pSnapshot){
  sqlite3_free(pSnapshot);
}
#endif /* SQLITE_ENABLE_SNAPSHOT */

#ifndef SQLITE_OMIT_COMPILEOPTION_DIAGS
/*
** Given the name of a compile-time option, return true if that option
** was used and false if not.
**
** The name can optionally begin with "SQLITE_" but the "SQLITE_" prefix
** is not required for a match.
*/
int sqlite3_compileoption_used(const char *zOptName){
  int i, n;
  int nOpt;
  const char **azCompileOpt;
 
#if SQLITE_ENABLE_API_ARMOR
  if( zOptName==0 ){
    (void)SQLITE_MISUSE_BKPT;
    return 0;
  }
#endif

  azCompileOpt = sqlite3CompileOptions(&nOpt);

  if( sqlite3StrNICmp(zOptName, "SQLITE_", 7)==0 ) zOptName += 7;
  n = sqlite3Strlen30(zOptName);

  /* Since nOpt is normally in single digits, a linear search is 
  ** adequate. No need for a binary search. */
  for(i=0; i<nOpt; i++){
    if( sqlite3StrNICmp(zOptName, azCompileOpt[i], n)==0
     && sqlite3IsIdChar((unsigned char)azCompileOpt[i][n])==0
    ){
      return 1;
    }
  }
  return 0;
}

/*
** Return the N-th compile-time option string.  If N is out of range,
** return a NULL pointer.
*/
const char *sqlite3_compileoption_get(int N){
  int nOpt;
  const char **azCompileOpt;
  azCompileOpt = sqlite3CompileOptions(&nOpt);
  if( N>=0 && N<nOpt ){
    return azCompileOpt[N];
  }
  return 0;
}
#endif /* SQLITE_OMIT_COMPILEOPTION_DIAGS */
