%{
static	char	ami_lexer_l[] = "$Header$";
/**********************************************************************

  ami_lexer. 

  a lexical scanner for postgres' amiint
  recent extensions include $def for macro definitions
  and ${id} for macro evaluation

 **********************************************************************/

#include "bootstrap/bkint.h"
#include "tmp/portal.h" 

#undef BOOTSTRAP
#include "bootstrap.h"

int	yylval;
int yycolumn,yyline;
#define NEWLINE() {yycolumn=0;yyline++;}

%}

%a 5000
%o 10000

D	[0-9]
oct     \\{D}{D}{D}
Exp	[Ee][-+]?{D}+
id      ([A-Za-z0-9_]|{oct}|\-)+
sid     \"([^\"])*\"
arrayid	[A-Za-z0-9_]+\[{D}*\]

%%

o |
open       	return(OPEN);

c |
close		return(XCLOSE);

C |
create		return(XCREATE);

p |
print		return(P_RELN);

in		return(XIN);
as		return(AS);
to		return(XTO);
OID             return(OBJ_ID);
bootstrap	return(BOOTSTRAP); 
_null_		return(NULLVAL);

i |
insert		return(INSERT_TUPLE);

q |
quit		return(QUIT);

\%pstr          {printstrtable();EMITPROMPT;}

\%phash     	{printhashtable();EMITPROMPT;}

".D" |
destroy		{return(XDESTROY);}

".R" |
rename		{return(XRENAME);}
attribute	{return(ATTR);}
relation 	{return(RELATION); }
","     	{return(COMMA);}
":"		{return(COLON);}
"="		{return(EQUALS);}
"("		{return(LPAREN);}
")"		{return(RPAREN);}
add		{return(ADD);}

[\n]      	NEWLINE();
[\t]		;
" "		; 

^\#[^\n]* ; /*drop everything after "#" for comments */


"declare"	{return(XDECLARE);}
"build"		{return(XBUILD);}
"indices"	{return(INDICES);}
"macro"		{return(MACRO);}
"index"		{return(INDEX);}
"on"		{return(ON);}
"using"		{return(USING);}
"display"	{return(DISPLAY);}
"show"		{return(SHOW);}
"$"{id}		{ 
		  char *in, *out;
		  for(in=out=yytext; *out = *in; out++)
		    if(*in++ == '\\') *out = (unsigned char)MapEscape(&in);
		  *(out+1) = '\000';
		  yylval=LookUpMacro(&(yytext[1]));
		  return(ID);}
{arrayid}	{
			char last, this, *p;

			/* XXX arrays of "basetype" are always "_basetype".
			 *     this is an evil hack inherited from rel. 3.1.
			 * XXX array dimension is thrown away because we
			 *     don't support fixed-dimension arrays.  again,
			 *     sickness from 3.1.
			 */
			for (p = yytext, last = '_'; *p && *p != '['; ++p) {
				this = *p;
				*p = last;
				last = this;
			}
			*p++ = last;
			*p = '\0';
			yylval=EnterString(yytext); return(ID);
		}
{id}	 	 { 
		  char *in, *out;
		  for(in = out = yytext; *out = *in; out++)
		    if(*in++ == '\\') *out = (unsigned char)MapEscape(&in);
		   *(out+1) = '\000';
		   yylval=EnterString(yytext); return(ID);}
{sid}		 {
   		  char *in;
		  for (in = yytext + 1; *in != '\"'; ++in);
		  *in = 0;
		  yylval=EnterString(yytext+1); return(ID);}



(-)?{D}+"."{D}*({Exp})?	|
(-)?{D}*"."{D}+({Exp})?	|
(-)?{D}+{Exp}		{
			 yylval=EnterString(yytext);
			 return(FLOAT);
			}


(-)?{D}+	{
		 yylval=EnterString(yytext);
		 return(INT);
		}


.	printf("syntax error %d : -> %s\n", yylineno, yytext);



%%

yywrap()
{
	StartTransactionCommand();
	cleanup();
	CommitTransactionCommand();
	return;
}

yyerror(str)
	char *str;
{
	fprintf(stderr,"\tsyntax error %d : %s",yylineno, str);
}

