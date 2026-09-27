#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <glob.h>
#include <termios.h>
#include <limits.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

typedef struct { int line, col; } Pos;
static void Fatal(Pos p, const char *fmt, ...) { va_list ap; fprintf(stderr,"cs:%d:%d: error: ",p.line,p.col); va_start(ap,fmt); vfprintf(stderr,fmt,ap); va_end(ap); fputc('\n',stderr); exit(2); }
static char *Dup(const char *s){ char *p=strdup(s?s:""); if(!p){perror("strdup");exit(2);} return p; }
static char *DupN(const char *s,size_t n){ char *p=malloc(n+1); if(!p){perror("malloc");exit(2);} memcpy(p,s,n);p[n]=0;return p; }

typedef enum { T_EOF,T_NL,T_ID,T_NUM,T_STR,T_LP,T_RP,T_LB,T_RB,T_SEMI,T_COMMA,T_COLON,T_ASSIGN,T_INC,T_PLUS,T_MINUS,T_STAR,T_SLASH,T_MOD,T_EQ,T_NE,T_LT,T_LE,T_GT,T_DGT,T_GE,T_AND,T_PIPE,T_AMP,T_DOT,T_QMARK,T_LBRACK,T_RBRACK,T_OR,T_NOT,
	T_IF,T_ELSE,T_WHILE,T_FOR,T_SWITCH,T_CASE,T_DEFAULT,T_BREAK,T_CONTINUE,T_RETURN } Kind;
typedef struct { Kind k; Pos p; char *s; long long n; } Tok;

typedef struct { const char *s; size_t n,i; int line,col; } Lexer;
static int Peek(Lexer *l){ return l->i<l->n?(unsigned char)l->s[l->i]:0; }
static int Get(Lexer *l){ int c=Peek(l); if(!c)return 0; l->i++; if(c=='\n'){l->line++;l->col=1;}else l->col++; return c; }
static Kind Keyword(const char *s){
	if(!strcmp(s,"if"))return T_IF; if(!strcmp(s,"else"))return T_ELSE; if(!strcmp(s,"while"))return T_WHILE; if(!strcmp(s,"for"))return T_FOR;
	if(!strcmp(s,"switch"))return T_SWITCH; if(!strcmp(s,"case"))return T_CASE; if(!strcmp(s,"default"))return T_DEFAULT; if(!strcmp(s,"break"))return T_BREAK; if(!strcmp(s,"continue"))return T_CONTINUE; if(!strcmp(s,"return"))return T_RETURN; return T_ID;
}
static Tok Next(Lexer *l){
	for(;;){ int c=Peek(l); if(!c)return (Tok){.k=T_EOF,.p={l->line,l->col},.s=NULL,.n=0}; if(c==' '||c=='\t'||c=='\r'){Get(l);continue;} if(c=='#'){while(Peek(l)&&Peek(l)!='\n')Get(l);continue;} break; }
	Pos p={l->line,l->col}; int c=Get(l); Tok t={0};t.p=p;
	if(c=='\n')t.k=T_NL; else if(c=='(')t.k=T_LP; else if(c==')')t.k=T_RP; else if(c=='{')t.k=T_LB; else if(c=='}')t.k=T_RB; else if(c==';')t.k=T_SEMI; else if(c==',')t.k=T_COMMA; else if(c==':')t.k=T_COLON; else if(c=='.')t.k=T_DOT; else if(c=='?')t.k=T_QMARK; else if(c=='[')t.k=T_LBRACK; else if(c==']')t.k=T_RBRACK;
	else if(c=='+'){if(Peek(l)=='+'){Get(l);t.k=T_INC;}else t.k=T_PLUS;} else if(c=='-')t.k=T_MINUS; else if(c=='*')t.k=T_STAR; else if(c=='/')t.k=T_SLASH; else if(c=='%')t.k=T_MOD;
	else if(c=='='){if(Peek(l)=='='){Get(l);t.k=T_EQ;}else t.k=T_ASSIGN;} else if(c=='!'){if(Peek(l)=='='){Get(l);t.k=T_NE;}else t.k=T_NOT;}
	else if(c=='<'){if(Peek(l)=='='){Get(l);t.k=T_LE;}else t.k=T_LT;} else if(c=='>'){if(Peek(l)=='='){Get(l);t.k=T_GE;}else if(Peek(l)=='>'){Get(l);t.k=T_DGT;}else t.k=T_GT;}
	else if(c=='&'){if(Peek(l)=='&'){Get(l);t.k=T_AND;}else t.k=T_AMP;} else if(c=='|'){if(Peek(l)=='|'){Get(l);t.k=T_OR;}else t.k=T_PIPE;}
	else if(c=='"' || c=='\''){ int quote=c; size_t cap=64,len=0; t.s=malloc(cap); if(!t.s)exit(2); for(;;){int d=Get(l); if(!d)Fatal(p,"unterminated string"); if(d==quote)break; if(quote=='"' && d=='\\'){d=Get(l);if(!d)Fatal(p,"unterminated escape"); if(d=='n')d='\n';else if(d=='t')d='\t';else if(d=='r')d='\r';else if(d=='"')d='"';else if(d=='\\')d='\\';} if(len+2>cap){cap*=2;t.s=realloc(t.s,cap);} t.s[len++]=(char)d;}t.s[len]=0;t.k=T_STR; }
	else if(isdigit(c)){ long long v=c-'0';while(isdigit(Peek(l))){v=v*10+(Get(l)-'0');}t.k=T_NUM;t.n=v; }
	else if(isalpha(c)||c=='_'){size_t a=l->i-1;while(isalnum(Peek(l))||Peek(l)=='_')Get(l);t.s=DupN(l->s+a,l->i-a);t.k=Keyword(t.s);}
	else Fatal(p,"unexpected character '%c'",c);
	return t;
}
static void FreeTok(Tok *t){free(t->s);t->s=NULL;}

typedef struct { char *name,*value; } Var;
typedef struct { Var *v; size_t n,cap; } Vars;
static int VarIndex(Vars *v,const char *n){for(size_t i=0;i<v->n;i++)if(!strcmp(v->v[i].name,n))return (int)i;return -1;}
static const char *VarGet(Vars *v,const char *n){int i=VarIndex(v,n);return i<0?NULL:v->v[i].value;}
static void VarSetRaw(Vars *v,const char *n,const char *x){int i=VarIndex(v,n);if(i<0){if(v->n==v->cap){v->cap=v->cap?v->cap*2:32;v->v=realloc(v->v,v->cap*sizeof(*v->v));}i=(int)v->n++;v->v[i].name=Dup(n);v->v[i].value=NULL;}free(v->v[i].value);v->v[i].value=Dup(x);}
static bool VarUnset(Vars *v,const char *n){int i=VarIndex(v,n);if(i<0)return false;free(v->v[i].name);free(v->v[i].value);v->v[i]=v->v[--v->n];return true;}

typedef enum { E_NUM,E_STR,E_VAR,E_ASSIGN,E_INC,E_UNARY,E_BINARY } EKind;
typedef struct Expr Expr;struct Expr{EKind k;Pos p;union{long long n;char *s,*name;struct{char *name;Expr *rhs;}assign;struct{char *name;}inc;struct{int op;Expr *a;}unary;struct{int op;Expr *a,*b;}binary;}u;};
static Expr *ENew(EKind k,Pos p){Expr *e=calloc(1,sizeof(*e));if(!e)exit(2);e->k=k;e->p=p;return e;}
static void EFree(Expr *e){if(!e)return;switch(e->k){case E_STR:free(e->u.s);break;case E_VAR:free(e->u.name);break;case E_ASSIGN:free(e->u.assign.name);EFree(e->u.assign.rhs);break;case E_INC:free(e->u.inc.name);break;case E_UNARY:EFree(e->u.unary.a);break;case E_BINARY:EFree(e->u.binary.a);EFree(e->u.binary.b);break;default:break;}free(e);}

typedef struct Stmt Stmt;typedef struct{Stmt **a;size_t n,cap;} Stmts;static void SPush(Stmts *s,Stmt *x){if(s->n==s->cap){s->cap=s->cap?s->cap*2:16;s->a=realloc(s->a,s->cap*sizeof(*s->a));}s->a[s->n++]=x;}
typedef struct{char *s;bool quoted;bool number;} Arg;typedef struct{Arg *a;size_t n,cap;} Args;typedef struct{int fd;int mode;Arg target;} Redir;typedef struct{Redir *a;size_t n,cap;} Redirs;static void RPush(Redirs*r,Redir x){if(r->n==r->cap){r->cap=r->cap?r->cap*2:4;r->a=realloc(r->a,r->cap*sizeof(*r->a));}r->a[r->n++]=x;}static void APush(Args *a,Arg x){if(a->n==a->cap){a->cap=a->cap?a->cap*2:8;a->a=realloc(a->a,a->cap*sizeof(*a->a));}a->a[a->n++]=x;}
typedef struct{Expr *value;Stmts body;} Case;
typedef struct{Case *a;size_t n,cap;Stmts def;bool hasDef;} Cases;static void CPush(Cases *c,Case x){if(c->n==c->cap){c->cap=c->cap?c->cap*2:8;c->a=realloc(c->a,c->cap*sizeof(*c->a));}c->a[c->n++]=x;}
typedef struct{char **names;size_t n,cap;} Params;static void PPush(Params*p,char*s){if(p->n==p->cap){p->cap=p->cap?p->cap*2:8;p->names=realloc(p->names,p->cap*sizeof(*p->names));}p->names[p->n++]=s;}
typedef struct{char *name;Params params;Stmts body;} Func;typedef struct{Func *a;size_t n,cap;} Funcs;typedef struct{char *name,*value;} Alias;typedef struct{Alias *a;size_t n,cap;} Aliases;typedef struct{char **a;size_t n,cap;} History;static Alias *FindAlias(Aliases*a,const char*n){for(size_t i=0;i<a->n;i++)if(!strcmp(a->a[i].name,n))return &a->a[i];return NULL;}static void SetAlias(Aliases*a,const char*n,const char*v){Alias*x=FindAlias(a,n);if(!x){if(a->n==a->cap){a->cap=a->cap?a->cap*2:16;a->a=realloc(a->a,a->cap*sizeof(*a->a));}x=&a->a[a->n++];x->name=Dup(n);x->value=NULL;}free(x->value);x->value=Dup(v);}static bool Unalias(Aliases*a,const char*n){for(size_t i=0;i<a->n;i++)if(!strcmp(a->a[i].name,n)){free(a->a[i].name);free(a->a[i].value);a->a[i]=a->a[--a->n];return true;}return false;}static void HistoryPush(History*h,const char*s){if(!*s)return;if(h->n==h->cap){h->cap=h->cap?h->cap*2:64;h->a=realloc(h->a,h->cap*sizeof(*h->a));}h->a[h->n++]=Dup(s);}static void FPush(Funcs*f,Func x){if(f->n==f->cap){f->cap=f->cap?f->cap*2:16;f->a=realloc(f->a,f->cap*sizeof(*f->a));}f->a[f->n++]=x;}

typedef enum{S_BLOCK,S_IF,S_WHILE,S_FOR,S_SWITCH,S_BREAK,S_CONTINUE,S_RETURN,S_EXPR,S_CMD,S_PIPE,S_BACKGROUND,S_FUNCDEF,S_FUNCCALL} SKind;
struct Stmt{SKind k;Pos p;union{Stmts block;struct{Expr*c;Stmts a,b;bool hasElse;}ifs;struct{Expr*c;Stmts b;}wh;struct{Expr*i,*c,*p;Stmts b;}fr;struct{Expr*x;Cases cs;}sw;Expr*expr;struct{Args args;Redirs redirs;}cmd;Stmts pipe;Stmt *background;struct{Func f;}fd;struct{char *name;Args args;}call;}u;};
static Stmt *SNew(SKind k,Pos p){Stmt*s=calloc(1,sizeof(*s));if(!s)exit(2);s->k=k;s->p=p;return s;}
typedef struct Parser{Lexer l;Tok t,n;}Parser;
static void Bump(Parser*p){FreeTok(&p->t);p->t=p->n;p->n=Next(&p->l);}static bool Is(Parser*p,Kind k){return p->t.k==k;}static void Need(Parser*p,Kind k,const char*w){if(!Is(p,k))Fatal(p->t.p,"expected %s",w);Bump(p);}static void SkipNL(Parser*p){while(Is(p,T_NL)||Is(p,T_SEMI))Bump(p);}
static int Prec(Kind k){switch(k){case T_OR:return 1;case T_AND:return 2;case T_EQ:case T_NE:return 3;case T_LT:case T_LE:case T_GT:case T_GE:return 4;case T_PLUS:case T_MINUS:return 5;case T_STAR:case T_SLASH:case T_MOD:return 6;default:return 0;}}
static int BOp(Kind k){return (int)k;}
static Expr *ParseExpr(Parser*p);static Stmts ParseBlock(Parser*p);static Stmt *ParseStmt(Parser*p);
static Expr *Primary(Parser*p){Pos q=p->t.p;if(Is(p,T_NUM)){Expr*e=ENew(E_NUM,q);e->u.n=p->t.n;Bump(p);return e;}if(Is(p,T_STR)){Expr*e=ENew(E_STR,q);e->u.s=p->t.s;p->t.s=NULL;Bump(p);return e;}if(Is(p,T_ID)){Expr*e=ENew(E_VAR,q);e->u.name=p->t.s;p->t.s=NULL;Bump(p);return e;}if(Is(p,T_LP)){Bump(p);Expr*e=ParseExpr(p);Need(p,T_RP,"')'");return e;}Fatal(q,"expected expression");return NULL;}
static Expr *Unary(Parser*p){Pos q=p->t.p;if(Is(p,T_INC)){Bump(p);if(!Is(p,T_ID))Fatal(q,"expected variable after ++");Expr*e=ENew(E_INC,q);e->u.inc.name=p->t.s;p->t.s=NULL;Bump(p);return e;}if(Is(p,T_NOT)||Is(p,T_MINUS)){Kind k=p->t.k;Bump(p);Expr*e=ENew(E_UNARY,q);e->u.unary.op=(int)k;e->u.unary.a=Unary(p);return e;}return Primary(p);}
static Expr *Bin(Parser*p,int min,Expr*lhs){for(;;){int pr=Prec(p->t.k);if(pr<min)return lhs;Kind op=p->t.k;Pos q=p->t.p;Bump(p);Expr*r=Unary(p);int np=Prec(p->t.k);if(np>pr)r=Bin(p,pr+1,r);Expr*e=ENew(E_BINARY,q);e->u.binary.op=BOp(op);e->u.binary.a=lhs;e->u.binary.b=r;lhs=e;}}
static Expr *ParseExpr(Parser*p){Expr*l=Unary(p);if(l->k==E_VAR&&Is(p,T_ASSIGN)){char *n=l->u.name;Pos q=l->p;free(l);Bump(p);Expr*e=ENew(E_ASSIGN,q);e->u.assign.name=n;e->u.assign.rhs=ParseExpr(p);return e;}return Bin(p,1,l);}

static bool StartsExpr(Parser*p){return Is(p,T_INC)||Is(p,T_NOT)||Is(p,T_MINUS)||Is(p,T_LP)||Is(p,T_NUM)||Is(p,T_STR)||(Is(p,T_ID)&&(p->n.k==T_ASSIGN));}
static Stmts ParseBlock(Parser*p){Stmts s={0};Need(p,T_LB,"'{'");while(!Is(p,T_RB)){if(Is(p,T_EOF))Fatal(p->t.p,"unterminated block");SkipNL(p);if(Is(p,T_RB))break;SPush(&s,ParseStmt(p));}Need(p,T_RB,"'}'");return s;}
static Args ParseArgs(Parser*p, bool paren){Args a={0};if(paren){Need(p,T_LP,"'('");if(Is(p,T_RP)){Bump(p);return a;}}for(;;){if(Is(p,T_EOF)||Is(p,T_NL)||Is(p,T_SEMI)||Is(p,T_RP))break;if(Is(p,T_COMMA)){Bump(p);continue;}if(Is(p,T_ID)||Is(p,T_NUM)||Is(p,T_STR)){Arg x={0};if(Is(p,T_STR)){x.s=p->t.s;p->t.s=NULL;}else{x.s=Dup(Is(p,T_ID)?p->t.s:"");}if(Is(p,T_NUM)){char b[64];snprintf(b,sizeof b,"%lld",p->t.n);free(x.s);x.s=Dup(b);x.number=true;}x.quoted=Is(p,T_STR);APush(&a,x);Bump(p);continue;}Fatal(p->t.p,"invalid command/function argument");}if(paren)Need(p,T_RP,"')'");return a;}
static bool WordFragment(Kind k){return k==T_ID||k==T_NUM||k==T_MINUS||k==T_PLUS||k==T_STAR||k==T_SLASH||k==T_MOD||k==T_COLON||k==T_COMMA||k==T_ASSIGN||k==T_DOT||k==T_QMARK||k==T_LBRACK||k==T_RBRACK;}
static char FragmentChar(Kind k){switch(k){case T_MINUS:return '-';case T_PLUS:return '+';case T_STAR:return '*';case T_SLASH:return '/';case T_MOD:return '%';case T_COLON:return ':';case T_COMMA:return ',';case T_ASSIGN:return '=';case T_DOT:return '.';case T_QMARK:return '?';case T_LBRACK:return '[';case T_RBRACK:return ']';default:return 0;}}
static Arg ParseCommandArg(Parser*p){if(Is(p,T_STR)){Arg x={p->t.s,true,false};p->t.s=NULL;Bump(p);return x;}if(!WordFragment(p->t.k))Fatal(p->t.p,"expected command argument");size_t cap=64,len=0;char*out=malloc(cap);if(!out)exit(2);size_t endcol=0;for(;;){Tok*t=&p->t;if(t->k==T_ID){size_t n=strlen(t->s);if(len+n+1>cap){while(len+n+1>cap)cap*=2;out=realloc(out,cap);}memcpy(out+len,t->s,n);len+=n;endcol=t->p.col+(int)n;}else if(t->k==T_NUM){char b[64];snprintf(b,sizeof b,"%lld",t->n);size_t n=strlen(b);if(len+n+1>cap){while(len+n+1>cap)cap*=2;out=realloc(out,cap);}memcpy(out+len,b,n);len+=n;endcol=t->p.col+(int)n;}else{char c=FragmentChar(t->k);if(!c)break;if(len+2>cap){cap*=2;out=realloc(out,cap);}out[len++]=c;endcol=t->p.col+1;}Bump(p);if(!WordFragment(p->t.k)||p->t.p.col!=(int)endcol)break;}out[len]=0;return (Arg){out,false,false};}

static bool IsRedirOp(Kind k){return k==T_LT||k==T_GT||k==T_DGT;}
static Stmt *ParseCommand(Parser*p){Pos q=p->t.p;Stmt*s=SNew(S_CMD,q);for(;;){if(Is(p,T_EOF)||Is(p,T_NL)||Is(p,T_SEMI)||Is(p,T_RB)||Is(p,T_PIPE)||Is(p,T_AMP))break;int fd=-1;Kind op=T_EOF;if(Is(p,T_NUM)&&(p->n.k==T_LT||p->n.k==T_GT||p->n.k==T_DGT)){fd=(int)p->t.n;Bump(p);op=p->t.k;Bump(p);}else if(IsRedirOp(p->t.k)){op=p->t.k;fd=op==T_LT?0:1;Bump(p);}if(op!=T_EOF){if(!(Is(p,T_ID)||Is(p,T_NUM)||Is(p,T_STR)))Fatal(p->t.p,"expected redirection target");Redir r={0};r.fd=fd;r.mode=op==T_LT?0:(op==T_DGT?2:1);r.target=ParseCommandArg(p);RPush(&s->u.cmd.redirs,r);continue;}if(!WordFragment(p->t.k)&&!Is(p,T_STR))Fatal(p->t.p,"invalid command argument");APush(&s->u.cmd.args,ParseCommandArg(p));}if(!s->u.cmd.args.n)Fatal(q,"empty command");if(Is(p,T_SEMI))Bump(p);else if(!Is(p,T_NL)&&!Is(p,T_RB)&&!Is(p,T_EOF)&&!Is(p,T_PIPE)&&!Is(p,T_AMP))Fatal(p->t.p,"expected end of command");return s;}
static Stmt *ParsePipeline(Parser*p){Stmt*first=ParseCommand(p);if(!Is(p,T_PIPE))return first;Stmt*s=SNew(S_PIPE,first->p);SPush(&s->u.pipe,first);while(Is(p,T_PIPE)){Bump(p);if(Is(p,T_NL)||Is(p,T_SEMI)||Is(p,T_RB)||Is(p,T_EOF))Fatal(p->t.p,"expected command after '|'");SPush(&s->u.pipe,ParseCommand(p));}return s;}
static bool LooksFuncDef(Parser*p){
	if(!Is(p,T_ID)||(p->n.k!=T_LP)) return false;
	Lexer l=p->l; int depth=1; Tok t=Next(&l);
	while(t.k!=T_EOF){
		if(t.k==T_LP) depth++;
		else if(t.k==T_RP){depth--; if(depth==0){FreeTok(&t); Tok after=Next(&l); bool result=after.k==T_LB; FreeTok(&after); return result;}}
		FreeTok(&t); t=Next(&l);
	}
	FreeTok(&t); return false;
}
static Stmt *ParseFunction(Parser*p){Pos q=p->t.p;char *name=p->t.s;p->t.s=NULL;Bump(p);Need(p,T_LP,"'('");Params ps={0};while(!Is(p,T_RP)){if(!Is(p,T_ID))Fatal(p->t.p,"expected parameter name");PPush(&ps,p->t.s);p->t.s=NULL;Bump(p);if(Is(p,T_COMMA))Bump(p);else if(!Is(p,T_RP))Fatal(p->t.p,"expected ',' or ')' ");}Need(p,T_RP,"')'");SkipNL(p);Stmts body=ParseBlock(p);Stmt*s=SNew(S_FUNCDEF,q);s->u.fd.f=(Func){name,ps,body};return s;}
static Stmt *ParseCall(Parser*p){Pos q=p->t.p;char *name=p->t.s;p->t.s=NULL;Bump(p);Args a=ParseArgs(p,true);if(Is(p,T_SEMI)||Is(p,T_NL))Bump(p);Stmt*s=SNew(S_FUNCCALL,q);s->u.call.name=name;s->u.call.args=a;return s;}
static Stmt *ParseSwitch(Parser*p){
	Pos q=p->t.p; Bump(p); Need(p,T_LP,"'('"); Expr*x=ParseExpr(p); Need(p,T_RP,"')'"); SkipNL(p); Need(p,T_LB,"'{'");
	Stmt*s=SNew(S_SWITCH,q); s->u.sw.x=x;
	while(!Is(p,T_RB)){
		SkipNL(p);
		if(Is(p,T_RB)) break;
		if(Is(p,T_CASE)){
			Bump(p); Case c={0}; c.value=ParseExpr(p); Need(p,T_COLON,"':'");
			if(Is(p,T_LB)) c.body=ParseBlock(p);
			else {
				while(!Is(p,T_CASE)&&!Is(p,T_DEFAULT)&&!Is(p,T_RB)&&!Is(p,T_EOF)) { SPush(&c.body,ParseStmt(p)); SkipNL(p); }
			}
			CPush(&s->u.sw.cs,c); continue;
		}
		if(Is(p,T_DEFAULT)){
			Bump(p); Need(p,T_COLON,"':'"); s->u.sw.cs.hasDef=true;
			if(Is(p,T_LB)) s->u.sw.cs.def=ParseBlock(p);
			else {
				while(!Is(p,T_CASE)&&!Is(p,T_DEFAULT)&&!Is(p,T_RB)&&!Is(p,T_EOF)) { SPush(&s->u.sw.cs.def,ParseStmt(p)); SkipNL(p); }
			}
			continue;
		}
		Fatal(p->t.p,"expected case or default");
	}
	Need(p,T_RB,"'}'"); return s;
}
static Stmt *ParseStmt(Parser*p){SkipNL(p);if(Is(p,T_IF)){Pos q=p->t.p;Bump(p);Need(p,T_LP,"'('");Expr*c=ParseExpr(p);Need(p,T_RP,"')'");SkipNL(p);Stmt*s=SNew(S_IF,q);s->u.ifs.c=c;s->u.ifs.a=ParseBlock(p);SkipNL(p);if(Is(p,T_ELSE)){Bump(p);s->u.ifs.hasElse=true;s->u.ifs.b=ParseBlock(p);}return s;}
	if(Is(p,T_WHILE)){Pos q=p->t.p;Bump(p);Need(p,T_LP,"'('");Expr*c=ParseExpr(p);Need(p,T_RP,"')'");SkipNL(p);Stmt*s=SNew(S_WHILE,q);s->u.wh.c=c;s->u.wh.b=ParseBlock(p);return s;}
	if(Is(p,T_FOR)){Pos q=p->t.p;Bump(p);Need(p,T_LP,"'('");Expr*i=NULL,*c=NULL,*r=NULL;if(!Is(p,T_SEMI))i=ParseExpr(p);Need(p,T_SEMI,"';'");if(!Is(p,T_SEMI))c=ParseExpr(p);Need(p,T_SEMI,"';'");if(!Is(p,T_RP))r=ParseExpr(p);Need(p,T_RP,"')'");SkipNL(p);Stmt*s=SNew(S_FOR,q);s->u.fr.i=i;s->u.fr.c=c;s->u.fr.p=r;s->u.fr.b=ParseBlock(p);return s;}
	if(Is(p,T_SWITCH))return ParseSwitch(p);if(Is(p,T_BREAK)){Stmt*s=SNew(S_BREAK,p->t.p);Bump(p);if(Is(p,T_SEMI)||Is(p,T_NL))Bump(p);return s;}if(Is(p,T_CONTINUE)){Stmt*s=SNew(S_CONTINUE,p->t.p);Bump(p);if(Is(p,T_SEMI)||Is(p,T_NL))Bump(p);return s;}if(Is(p,T_RETURN)){Stmt*s=SNew(S_RETURN,p->t.p);Bump(p);if(Is(p,T_NL)||Is(p,T_SEMI)){s->u.expr=NULL;Bump(p);}else{s->u.expr=ParseExpr(p);if(Is(p,T_NL)||Is(p,T_SEMI))Bump(p);}return s;}
	if(Is(p,T_LB)){Stmt*s=SNew(S_BLOCK,p->t.p);s->u.block=ParseBlock(p);return s;}if(LooksFuncDef(p))return ParseFunction(p);if(Is(p,T_ID)&&(p->n.k==T_LP))return ParseCall(p);if(StartsExpr(p)){Stmt*s=SNew(S_EXPR,p->t.p);s->u.expr=ParseExpr(p);if(Is(p,T_SEMI)||Is(p,T_NL))Bump(p);else if(!Is(p,T_RB)&&!Is(p,T_EOF))Fatal(p->t.p,"expected end of expression");return s;}if(Is(p,T_ID)||Is(p,T_NUM)||Is(p,T_STR)){Stmt*s=ParsePipeline(p);if(Is(p,T_AMP)){Bump(p);Stmt*b=SNew(S_BACKGROUND,s->p);b->u.background=s;return b;}return s;}return ParseCommand(p);}
	static Stmts ParseProgram(Parser*p){Stmts s={0};while(!Is(p,T_EOF)){SkipNL(p);if(Is(p,T_EOF))break;SPush(&s,ParseStmt(p));}return s;}

	static long long ToInt(const char*s,Pos p){char*e=NULL;errno=0;long long x=strtoll(s,&e,10);if(errno||!e||*e)Fatal(p,"'%s' is not an integer",s);return x;}
	typedef struct{bool isInt,isBool;long long n;bool b;char *s;} Value;
	static void VFree(Value*v){free(v->s);v->s=NULL;}static char *VStr(Value v){if(v.isInt){char b[64];snprintf(b,sizeof b,"%lld",v.n);return Dup(b);}if(v.isBool)return Dup(v.b?"1":"0");return Dup(v.s?v.s:"");}static bool Truth(Value v){if(v.isBool)return v.b;if(v.isInt)return v.n!=0;return v.s&&*v.s;}

	typedef enum{FLOW_NONE,FLOW_BREAK,FLOW_CONTINUE,FLOW_RETURN,FLOW_EXIT} Flow;
	typedef enum{JOB_RUNNING,JOB_STOPPED,JOB_DONE} JobState;typedef struct{int id;pid_t pgid;char*command;JobState state;int status;} Job;typedef struct{Job*a;size_t n,cap;int next_id;} Jobs;typedef struct Runtime Runtime;struct Runtime{Vars vars;Funcs funcs;Aliases aliases;History history;Jobs jobs;pid_t shell_pgid;bool job_control;int status;Flow flow;int returnStatus;bool interactive;};
static Value Eval(Runtime*r,Expr*e);
static Value Eval(Runtime*r,Expr*e){Value z={0};switch(e->k){case E_NUM:z.isInt=true;z.n=e->u.n;return z;case E_STR:z.s=Dup(e->u.s);return z;case E_VAR:{const char*v=VarGet(&r->vars,e->u.name);if(!v)Fatal(e->p,"undefined variable '%s'",e->u.name);z.s=Dup(v);return z;}case E_ASSIGN:{Value v=Eval(r,e->u.assign.rhs);char *s=VStr(v);VFree(&v);VarSetRaw(&r->vars,e->u.assign.name,s);setenv(e->u.assign.name,s,1);z.s=Dup(s);free(s);return z;}case E_INC:{const char*v=VarGet(&r->vars,e->u.inc.name);if(!v)Fatal(e->p,"undefined variable '%s'",e->u.inc.name);long long n=ToInt(v,e->p)+1;char b[64];snprintf(b,sizeof b,"%lld",n);VarSetRaw(&r->vars,e->u.inc.name,b);setenv(e->u.inc.name,b,1);z.isInt=true;z.n=n;return z;}case E_UNARY:{Value a=Eval(r,e->u.unary.a);if(e->u.unary.op==T_NOT){z.isBool=true;z.b=!Truth(a);}else{long long n=a.isInt?a.n:ToInt(VStr(a),e->p);z.isInt=true;z.n=-n;}VFree(&a);return z;}case E_BINARY:{if(e->u.binary.op==T_AND){Value a=Eval(r,e->u.binary.a);bool t=Truth(a);VFree(&a);if(!t){z.isBool=true;z.b=false;return z;}Value b=Eval(r,e->u.binary.b);z.isBool=true;z.b=Truth(b);VFree(&b);return z;}if(e->u.binary.op==T_OR){Value a=Eval(r,e->u.binary.a);bool t=Truth(a);VFree(&a);if(t){z.isBool=true;z.b=true;return z;}Value b=Eval(r,e->u.binary.b);z.isBool=true;z.b=Truth(b);VFree(&b);return z;}Value a=Eval(r,e->u.binary.a),b=Eval(r,e->u.binary.b);int op=e->u.binary.op;long long ai=0,bi=0;bool an=false,bn=false;if(a.isInt){ai=a.n;an=true;}else if(a.s) {char*e1;ai=strtoll(a.s,&e1,10);an=*a.s&&!*e1;}if(b.isInt){bi=b.n;bn=true;}else if(b.s){char*e1;bi=strtoll(b.s,&e1,10);bn=*b.s&&!*e1;}if(op==T_PLUS||op==T_MINUS||op==T_STAR||op==T_SLASH||op==T_MOD||op==T_LT||op==T_LE||op==T_GT||op==T_GE){if(!an||!bn)Fatal(e->p,"numeric expression requires integer values");z.isInt=op<T_LT||op==T_PLUS||op==T_MINUS||op==T_STAR||op==T_SLASH||op==T_MOD;switch(op){case T_PLUS:z.n=ai+bi;break;case T_MINUS:z.n=ai-bi;break;case T_STAR:z.n=ai*bi;break;case T_SLASH:if(!bi)Fatal(e->p,"division by zero");z.n=ai/bi;break;case T_MOD:if(!bi)Fatal(e->p,"division by zero");z.n=ai%bi;break;case T_LT:z.isBool=true;z.b=ai<bi;break;case T_LE:z.isBool=true;z.b=ai<=bi;break;case T_GT:z.isBool=true;z.b=ai>bi;break;case T_GE:z.isBool=true;z.b=ai>=bi;break;}VFree(&a);VFree(&b);return z;}char *as=VStr(a),*bs=VStr(b);int cmp=strcmp(as,bs);z.isBool=true;z.b=(op==T_EQ)?cmp==0:(op==T_NE)?cmp!=0:false;VFree(&a);VFree(&b);free(as);free(bs);return z;}default:return z;}}

static void FreeArgs(Args*a){for(size_t i=0;i<a->n;i++)free(a->a[i].s);free(a->a);}
static int RunText(Runtime*r,const char*src);static void SetStatus(Runtime*r,int status){r->status=status;char b[32];snprintf(b,sizeof b,"%d",status);VarSetRaw(&r->vars,"status",b);setenv("status",b,1);}
static const char *ExpandArg(Runtime*r,Arg*x){if(x->quoted||x->number)return x->s;const char*v=VarGet(&r->vars,x->s);return v?v:x->s;}
static bool ApplyRedirs(Runtime*r,Redirs*rs){for(size_t i=0;i<rs->n;i++){Redir*x=&rs->a[i];const char*path=ExpandArg(r,&x->target);int flags=x->mode==0?O_RDONLY:(O_WRONLY|O_CREAT|(x->mode==2?O_APPEND:O_TRUNC));int fd=open(path,flags,0666);if(fd<0){fprintf(stderr,"cs: %s: %s\n",path,strerror(errno));return false;}if(dup2(fd,x->fd)<0){fprintf(stderr,"cs: dup2: %s\n",strerror(errno));close(fd);return false;}close(fd);}return true;}
static bool HasGlob(const char*s){return strpbrk(s,"*?[")!=NULL;}
static void ArgvPush(char***av,size_t*n,size_t*cap,const char*s){if(*n+1>=*cap){*cap*=2;*av=realloc(*av,*cap*sizeof(**av));}(*av)[(*n)++]=Dup(s);(*av)[*n]=NULL;}
static char **BuildArgv(Runtime*r,Args*a){size_t cap=a->n*2+8,n=0;char**av=calloc(cap,sizeof(char*));for(size_t i=0;i<a->n;i++){Arg*x=&a->a[i];const char*v=(i==0||x->quoted||x->number)?x->s:(VarGet(&r->vars,x->s)?VarGet(&r->vars,x->s):x->s);if(i>0&&!x->quoted&&HasGlob(v)){glob_t g={0};int rc=glob(v,GLOB_NOCHECK,NULL,&g);if(rc==0){for(size_t j=0;j<g.gl_pathc;j++)ArgvPush(&av,&n,&cap,g.gl_pathv[j]);globfree(&g);continue;}globfree(&g);}ArgvPush(&av,&n,&cap,v);}return av;}

static void FreeArgv(char**av,size_t n){if(n==0){for(size_t i=0;av&&av[i];i++)free(av[i]);}else for(size_t i=0;i<n;i++)free(av[i]);free(av);}
static Job *FindJob(Runtime*r,int id){for(size_t i=0;i<r->jobs.n;i++)if(r->jobs.a[i].id==id)return &r->jobs.a[i];return NULL;}
static Job *AddJob(Runtime*r,pid_t pgid,const char*cmd,JobState state){if(r->jobs.n==r->jobs.cap){r->jobs.cap=r->jobs.cap?r->jobs.cap*2:16;r->jobs.a=realloc(r->jobs.a,r->jobs.cap*sizeof(*r->jobs.a));}Job*j=&r->jobs.a[r->jobs.n++];j->id=r->jobs.next_id++;j->pgid=pgid;j->command=Dup(cmd?cmd:"");j->state=state;j->status=0;return j;}
static void UpdateJobs(Runtime*r){for(;;){int st;pid_t p=waitpid(-1,&st,WNOHANG|WUNTRACED|WCONTINUED);if(p<=0)break;for(size_t i=0;i<r->jobs.n;i++){Job*j=&r->jobs.a[i];if(kill(-j->pgid,0)<0&&errno==ESRCH&&j->pgid!=p)continue;if(j->pgid==getpgid(p)){if(WIFSTOPPED(st))j->state=JOB_STOPPED;else if(WIFCONTINUED(st))j->state=JOB_RUNNING;else if(WIFEXITED(st)||WIFSIGNALED(st)){j->state=JOB_DONE;j->status=WIFEXITED(st)?WEXITSTATUS(st):128+WTERMSIG(st);}break;}}}}
static void GiveTerminal(Runtime*r,pid_t pgid){if(r->job_control)tcsetpgrp(STDIN_FILENO,pgid);}
static void TakeTerminal(Runtime*r){if(r->job_control)tcsetpgrp(STDIN_FILENO,r->shell_pgid);}
static int WaitForeground(Runtime*r,pid_t pgid,pid_t lastpid){int last=0,st;for(;;){pid_t p=waitpid(-pgid,&st,WUNTRACED);if(p<0){if(errno==EINTR)continue;if(errno==ECHILD)break;last=1;break;}if(p==lastpid){if(WIFEXITED(st))last=WEXITSTATUS(st);else if(WIFSIGNALED(st))last=128+WTERMSIG(st);}if(WIFSTOPPED(st)){Job*j=AddJob(r,pgid,"",JOB_STOPPED);j->status=128+WSTOPSIG(st);break;}}TakeTerminal(r);return last;}
static int RunPipeline(Runtime*r,Stmts*pipeline,bool background){size_t n=pipeline->n;pid_t*pids=calloc(n,sizeof(*pids));pid_t pgid=0;int prev=-1;int last_status=0;for(size_t i=0;i<n;i++){Stmt*st=pipeline->a[i];int fds[2]={-1,-1};if(i+1<n&&pipe(fds)<0){fprintf(stderr,"pipe: %s\\n",strerror(errno));free(pids);if(prev>=0)close(prev);SetStatus(r,1);return r->status;}Args*a=&st->u.cmd.args;char**av=BuildArgv(r,a);pid_t pid=fork();if(pid<0){fprintf(stderr,"fork: %s\\n",strerror(errno));FreeArgv(av,0);if(prev>=0)close(prev);if(fds[0]>=0){close(fds[0]);close(fds[1]);}free(pids);SetStatus(r,1);return r->status;}if(pid==0){signal(SIGINT,SIG_DFL);signal(SIGTSTP,SIG_DFL);signal(SIGTTIN,SIG_DFL);signal(SIGTTOU,SIG_DFL);if(!pgid)pgid=getpid();setpgid(0,pgid);if(prev>=0){dup2(prev,STDIN_FILENO);close(prev);}if(i+1<n){close(fds[0]);dup2(fds[1],STDOUT_FILENO);close(fds[1]);}if(!ApplyRedirs(r,&st->u.cmd.redirs))_exit(1);execvp(av[0],av);fprintf(stderr,"%s: %s\\n",av[0],strerror(errno));_exit(errno==ENOENT?127:126);}if(!pgid)pgid=pid;setpgid(pid,pgid);pids[i]=pid;FreeArgv(av,0);if(prev>=0)close(prev);if(i+1<n){close(fds[1]);prev=fds[0];}else prev=-1;}if(background){char cmd[1024]="";for(size_t i=0;i<n;i++){if(i)strcat(cmd," | ");if(pipeline->a[i]->u.cmd.args.n)strncat(cmd,pipeline->a[i]->u.cmd.args.a[0].s,sizeof(cmd)-strlen(cmd)-1);}Job*j=AddJob(r,pgid,cmd,JOB_RUNNING);fprintf(stderr,"[%d] %d\\n",j->id,(int)pgid);free(pids);SetStatus(r,0);return 0;}GiveTerminal(r,pgid);last_status=WaitForeground(r,pgid,pids[n-1]);free(pids);SetStatus(r,last_status);return r->status;}
static int RunCommand(Runtime*r,Stmt*s){Args*a=&s->u.cmd.args;Redirs*rs=&s->u.cmd.redirs;if(!a->n)return 0;if(!a->a[0].quoted){Alias*al=FindAlias(&r->aliases,a->a[0].s);if(al){size_t cap=strlen(al->value)+1;for(size_t i=1;i<a->n;i++)cap+=strlen(a->a[i].s)+3;char*src=malloc(cap);src[0]=0;strcat(src,al->value);for(size_t i=1;i<a->n;i++){strcat(src," ");if(a->a[i].quoted){strcat(src,"\"");strcat(src,a->a[i].s);strcat(src,"\"");}else strcat(src,a->a[i].s);}int rc=RunText(r,src);free(src);return rc;}}char**av=BuildArgv(r,a);bool builtin=!strcmp(av[0],"cd")||!strcmp(av[0],"unset")||!strcmp(av[0],"history")||!strcmp(av[0],"alias")||!strcmp(av[0],"unalias")||!strcmp(av[0],"jobs")||!strcmp(av[0],"fg")||!strcmp(av[0],"bg")||!strcmp(av[0],"exit");int saved[3]={-1,-1,-1};if(builtin&&rs->n){for(int i=0;i<3;i++)saved[i]=dup(i);if(!ApplyRedirs(r,rs)){for(int i=0;i<3;i++){if(saved[i]>=0){dup2(saved[i],i);close(saved[i]);}}FreeArgv(av,0);SetStatus(r,1);return r->status;}}
if(!strcmp(av[0],"cd")){const char*d=a->n>1?ExpandArg(r,&a->a[1]):NULL;if(!d)d=getenv("HOME");if(!d)d="/";char oldcwd[PATH_MAX],newcwd[PATH_MAX];bool have_old=getcwd(oldcwd,sizeof oldcwd)!=NULL;if(chdir(d))SetStatus(r,1);else{if(have_old){VarSetRaw(&r->vars,"OLDPWD",oldcwd);setenv("OLDPWD",oldcwd,1);}if(getcwd(newcwd,sizeof newcwd)){VarSetRaw(&r->vars,"PWD",newcwd);setenv("PWD",newcwd,1);}SetStatus(r,0);}goto done_builtin;}
if(!strcmp(av[0],"unset")){if(a->n<2){fprintf(stderr,"unset: missing variable name\n");SetStatus(r,2);goto done_builtin;}int rc=0;for(size_t i=1;i<a->n;i++){if(!VarUnset(&r->vars,av[i]))rc=1;unsetenv(av[i]);}SetStatus(r,rc);goto done_builtin;}
if(!strcmp(av[0],"history")){for(size_t i=0;i<r->history.n;i++)printf("%zu %s\n",i+1,r->history.a[i]);SetStatus(r,0);goto done_builtin;}
if(!strcmp(av[0],"alias")){if(a->n==1){for(size_t i=0;i<r->aliases.n;i++)printf("alias %s = \"%s\"\n",r->aliases.a[i].name,r->aliases.a[i].value);SetStatus(r,0);goto done_builtin;}if(a->n==2){Alias*x=FindAlias(&r->aliases,av[1]);if(!x){SetStatus(r,1);goto done_builtin;}printf("alias %s = \"%s\"\n",x->name,x->value);SetStatus(r,0);goto done_builtin;}const char*name=av[1];const char*value=NULL;if(a->n>=4&&!strcmp(av[2],"="))value=av[3];else if(a->n>=3)value=av[2];else{SetStatus(r,2);goto done_builtin;}SetAlias(&r->aliases,name,value);SetStatus(r,0);goto done_builtin;}
if(!strcmp(av[0],"unalias")){if(a->n<2){SetStatus(r,2);goto done_builtin;}int rc=0;for(size_t i=1;i<a->n;i++)if(!Unalias(&r->aliases,av[i]))rc=1;SetStatus(r,rc);goto done_builtin;}
if(!strcmp(av[0],"jobs")){UpdateJobs(r);for(size_t i=0;i<r->jobs.n;i++){Job*j=&r->jobs.a[i];printf("[%d] %s %s\n",j->id,j->state==JOB_RUNNING?"Running":j->state==JOB_STOPPED?"Stopped":"Done",j->command);}SetStatus(r,0);goto done_builtin;}
if(!strcmp(av[0],"bg")){int id=a->n>1?atoi(av[1]):0;Job*j=FindJob(r,id);if(!j){fprintf(stderr,"bg: %d: no such job\n",id);SetStatus(r,1);goto done_builtin;}if(kill(-j->pgid,SIGCONT)<0){SetStatus(r,1);goto done_builtin;}j->state=JOB_RUNNING;SetStatus(r,0);goto done_builtin;}
if(!strcmp(av[0],"fg")){int id=a->n>1?atoi(av[1]):0;Job*j=FindJob(r,id);if(!j){fprintf(stderr,"fg: %d: no such job\n",id);SetStatus(r,1);goto done_builtin;}GiveTerminal(r,j->pgid);if(kill(-j->pgid,SIGCONT)<0&&errno!=ESRCH){TakeTerminal(r);SetStatus(r,1);goto done_builtin;}j->state=JOB_RUNNING;int st;for(;;){pid_t p=waitpid(-j->pgid,&st,WUNTRACED);if(p<0){if(errno==EINTR)continue;break;}if(WIFSTOPPED(st)){j->state=JOB_STOPPED;break;}if(WIFEXITED(st)||WIFSIGNALED(st)){j->state=JOB_DONE;j->status=WIFEXITED(st)?WEXITSTATUS(st):128+WTERMSIG(st);break;}}TakeTerminal(r);SetStatus(r,j->status);goto done_builtin;}
if(!strcmp(av[0],"exit")){int code=a->n>1?atoi(av[1]):r->status;r->flow=FLOW_EXIT;r->returnStatus=code;goto done_builtin;}
{pid_t p=fork();if(p<0){fprintf(stderr,"fork: %s\n",strerror(errno));SetStatus(r,1);goto done;}if(p==0){signal(SIGINT,SIG_DFL);if(!ApplyRedirs(r,rs))_exit(1);execvp(av[0],av);fprintf(stderr,"%s: %s\n",av[0],strerror(errno));_exit(errno==ENOENT?127:126);}int st;if(waitpid(p,&st,0)<0)SetStatus(r,1);else if(WIFEXITED(st))SetStatus(r,WEXITSTATUS(st));else if(WIFSIGNALED(st))SetStatus(r,128+WTERMSIG(st));}
goto done;
done_builtin:if(builtin&&rs->n){for(int i=0;i<3;i++){if(saved[i]>=0){dup2(saved[i],i);close(saved[i]);}}}
done:FreeArgv(av,0);return r->status;}
static Func *FindFunc(Runtime*r,const char*n){for(size_t i=0;i<r->funcs.n;i++)if(!strcmp(r->funcs.a[i].name,n))return &r->funcs.a[i];return NULL;}
static void Execute(Runtime*r,Stmts*s);
static void Execute(Runtime*r,Stmts*s){for(size_t i=0;i<s->n;i++){Stmt*x=s->a[i];if(r->flow!=FLOW_NONE)return;switch(x->k){case S_BLOCK:Execute(r,&x->u.block);break;case S_EXPR:{Value v=Eval(r,x->u.expr);VFree(&v);break;}case S_CMD:RunCommand(r,x);break;case S_PIPE:RunPipeline(r,&x->u.pipe,false);break;case S_BACKGROUND:{if(x->u.background->k==S_PIPE)RunPipeline(r,&x->u.background->u.pipe,true);else{Stmt*child=x->u.background;if(child->k==S_CMD){Stmts one={0};Stmt pipe_stmt={0};pipe_stmt.k=S_PIPE;pipe_stmt.p=child->p;pipe_stmt.u.pipe=one;SPush(&pipe_stmt.u.pipe,child);RunPipeline(r,&pipe_stmt.u.pipe,true);memset(&pipe_stmt.u.pipe,0,sizeof pipe_stmt.u.pipe);}else RunPipeline(r,&x->u.background->u.pipe,true);}break;}case S_IF:{Value v=Eval(r,x->u.ifs.c);bool t=Truth(v);VFree(&v);Execute(r,t?&x->u.ifs.a:&x->u.ifs.b);break;}case S_WHILE:for(;;){Value v=Eval(r,x->u.wh.c);bool t=Truth(v);VFree(&v);if(!t)break;Execute(r,&x->u.wh.b);if(r->flow==FLOW_CONTINUE){r->flow=FLOW_NONE;continue;}if(r->flow==FLOW_BREAK){r->flow=FLOW_NONE;break;}if(r->flow!=FLOW_NONE)return;}break;case S_FOR:{if(x->u.fr.i){Value v=Eval(r,x->u.fr.i);VFree(&v);}for(;;){if(x->u.fr.c){Value v=Eval(r,x->u.fr.c);bool t=Truth(v);VFree(&v);if(!t)break;}Execute(r,&x->u.fr.b);if(r->flow==FLOW_BREAK){r->flow=FLOW_NONE;break;}if(r->flow==FLOW_CONTINUE)r->flow=FLOW_NONE;else if(r->flow!=FLOW_NONE)return;if(x->u.fr.p){Value v=Eval(r,x->u.fr.p);VFree(&v);}}}break;case S_SWITCH:{Value v=Eval(r,x->u.sw.x);bool hit=false;for(size_t j=0;j<x->u.sw.cs.n;j++){Value k=Eval(r,x->u.sw.cs.a[j].value);char*a=VStr(v),*b=VStr(k);bool eq=!strcmp(a,b);free(a);free(b);VFree(&k);if(eq){hit=true;Execute(r,&x->u.sw.cs.a[j].body);break;}}if(!hit&&x->u.sw.cs.hasDef)Execute(r,&x->u.sw.cs.def);VFree(&v);if(r->flow==FLOW_BREAK)r->flow=FLOW_NONE;break;}case S_BREAK:r->flow=FLOW_BREAK;break;case S_CONTINUE:r->flow=FLOW_CONTINUE;break;case S_RETURN:{int code=0;if(x->u.expr){Value v=Eval(r,x->u.expr);code=(int)(v.isInt?v.n:ToInt(VStr(v),x->p));VFree(&v);}r->returnStatus=code;r->status=code;r->flow=FLOW_RETURN;break;}case S_FUNCDEF:break;case S_FUNCCALL:{Func*f=FindFunc(r,x->u.call.name);if(!f)Fatal(x->p,"unknown function '%s'",x->u.call.name);Vars old=r->vars;r->vars=(Vars){0};for(size_t j=0;j<f->params.n;j++){const char*v=j<x->u.call.args.n?x->u.call.args.a[j].s:"";if(j<x->u.call.args.n&&!x->u.call.args.a[j].quoted&&!x->u.call.args.a[j].number){const char*q=VarGet(&old,v);if(q)v=q;}VarSetRaw(&r->vars,f->params.names[j],v);}Execute(r,&f->body);int rc=r->returnStatus;r->flow=FLOW_NONE;r->status=rc;for(size_t j=0;j<r->vars.n;j++){free(r->vars.v[j].name);free(r->vars.v[j].value);}free(r->vars.v);r->vars=old;break;}}}}
static void FreeStmt(Stmt*s){if(!s)return;switch(s->k){case S_BLOCK:for(size_t i=0;i<s->u.block.n;i++)FreeStmt(s->u.block.a[i]);free(s->u.block.a);break;case S_IF:EFree(s->u.ifs.c);for(size_t i=0;i<s->u.ifs.a.n;i++)FreeStmt(s->u.ifs.a.a[i]);for(size_t i=0;i<s->u.ifs.b.n;i++)FreeStmt(s->u.ifs.b.a[i]);free(s->u.ifs.a.a);free(s->u.ifs.b.a);break;case S_WHILE:EFree(s->u.wh.c);for(size_t i=0;i<s->u.wh.b.n;i++)FreeStmt(s->u.wh.b.a[i]);free(s->u.wh.b.a);break;case S_FOR:EFree(s->u.fr.i);EFree(s->u.fr.c);EFree(s->u.fr.p);for(size_t i=0;i<s->u.fr.b.n;i++)FreeStmt(s->u.fr.b.a[i]);free(s->u.fr.b.a);break;case S_SWITCH:EFree(s->u.sw.x);for(size_t j=0;j<s->u.sw.cs.n;j++){EFree(s->u.sw.cs.a[j].value);for(size_t i=0;i<s->u.sw.cs.a[j].body.n;i++)FreeStmt(s->u.sw.cs.a[j].body.a[i]);free(s->u.sw.cs.a[j].body.a);}free(s->u.sw.cs.a);for(size_t i=0;i<s->u.sw.cs.def.n;i++)FreeStmt(s->u.sw.cs.def.a[i]);free(s->u.sw.cs.def.a);break;case S_EXPR:EFree(s->u.expr);break;case S_CMD:FreeArgs(&s->u.cmd.args);for(size_t i=0;i<s->u.cmd.redirs.n;i++)free(s->u.cmd.redirs.a[i].target.s);free(s->u.cmd.redirs.a);break;case S_PIPE:for(size_t i=0;i<s->u.pipe.n;i++)FreeStmt(s->u.pipe.a[i]);free(s->u.pipe.a);break;case S_BACKGROUND:FreeStmt(s->u.background);break;case S_FUNCCALL:free(s->u.call.name);FreeArgs(&s->u.call.args);break;default:break;}free(s);}

static char *ReadAll(FILE*f){size_t cap=8192,n=0;char*b=malloc(cap);for(;;){if(n+4096>=cap){cap*=2;b=realloc(b,cap);}size_t k=fread(b+n,1,4096,f);n+=k;if(k<4096)break;}b[n]=0;return b;}
static void SigInt(int x){(void)x;write(STDERR_FILENO,"\n",1);}
static void SetScriptArgs(Runtime*r,int argc,char**argv){char b[64];int count=argc>1?argc-2:0;snprintf(b,sizeof b,"%d",count);VarSetRaw(&r->vars,"argc",b);VarSetRaw(&r->vars,"arg0",argc>1?argv[1]:"cs");for(int i=1;i<=count;i++){snprintf(b,sizeof b,"arg%d",i);VarSetRaw(&r->vars,b,argv[i+1]);}}
static int RunText(Runtime*r,const char*src){Parser p={0};p.l.s=src;p.l.n=strlen(src);p.l.line=1;p.l.col=1;p.t=Next(&p.l);p.n=Next(&p.l);Stmts s=ParseProgram(&p);for(size_t i=0;i<s.n;i++){if(s.a[i]->k==S_FUNCDEF){Func f=s.a[i]->u.fd.f;bool exists=false;for(size_t j=0;j<r->funcs.n;j++)if(!strcmp(r->funcs.a[j].name,f.name)){exists=true;break;}if(!exists){FPush(&r->funcs,f);memset(&s.a[i]->u.fd.f,0,sizeof(s.a[i]->u.fd.f));}}}Execute(r,&s);for(size_t i=0;i<s.n;i++)FreeStmt(s.a[i]);free(s.a);FreeTok(&p.t);FreeTok(&p.n);return r->flow==FLOW_EXIT?r->returnStatus:r->status;}
static bool CompletionChar(char c){return isalnum((unsigned char)c)||strchr("_-.+/~",c)!=NULL;}
static size_t LongestPrefix(char **v,size_t n){if(!n)return 0;size_t p=strlen(v[0]);for(size_t i=1;i<n;i++){size_t j=0;while(j<p&&v[0][j]&&v[i][j]&&v[0][j]==v[i][j])j++;p=j;}return p;}
static char *CommandCompletion(const char *prefix){
 size_t cap=128,n=0;char **m=calloc(cap,sizeof(*m));const char *builtins[]={"cd","exit","unset","history","alias","unalias"};
 for(size_t i=0;i<sizeof(builtins)/sizeof(builtins[0]);i++)if(!strncmp(builtins[i],prefix,strlen(prefix))){if(n==cap){cap*=2;m=realloc(m,cap*sizeof(*m));}m[n++]=Dup(builtins[i]);}
 const char *path=getenv("PATH");if(path){char *copy=Dup(path),*save=NULL;for(char *dir=strtok_r(copy,":",&save);dir;dir=strtok_r(NULL,":",&save)){if(!*dir)dir=".";DIR*d=opendir(dir);if(!d)continue;struct dirent*e;while((e=readdir(d))){if(strncmp(e->d_name,prefix,strlen(prefix)))continue;char full[PATH_MAX];snprintf(full,sizeof full,"%s/%s",dir,e->d_name);if(access(full,X_OK))continue;bool seen=false;for(size_t k=0;k<n;k++)if(!strcmp(m[k],e->d_name)){seen=true;break;}if(seen)continue;if(n==cap){cap*=2;m=realloc(m,cap*sizeof(*m));}m[n++]=Dup(e->d_name);}closedir(d);}free(copy);}
 if(!n){free(m);return NULL;}size_t p=LongestPrefix(m,n);char*out=DupN(m[0],p);for(size_t k=0;k<n;k++)free(m[k]);free(m);return out;}
static int CompleteLine(char *buf,size_t *n,size_t cap){
 size_t start=*n;while(start&&CompletionChar(buf[start-1]))start--;size_t len=*n-start;char prefix[PATH_MAX];if(len>=sizeof(prefix))return 0;memcpy(prefix,buf+start,len);prefix[len]=0;
 bool command=true;for(size_t i=0;i<start;i++)if(!isspace((unsigned char)buf[i])){command=false;break;}
 char *completion=NULL;if(command)completion=CommandCompletion(prefix);else{glob_t g={0};char pattern[PATH_MAX];snprintf(pattern,sizeof pattern,"%s*",prefix);if(glob(pattern,0,NULL,&g)==0&&g.gl_pathc==1){const char*p=strrchr(g.gl_pathv[0],'/');completion=Dup(p?p+1:g.gl_pathv[0]);if(strchr(prefix,'/')){free(completion);completion=Dup(g.gl_pathv[0]);}}globfree(&g);}
 if(!completion||strlen(completion)<=len){free(completion);return 0;}size_t add=strlen(completion)-len;if(*n+add+1>=cap){free(completion);return 0;}memcpy(buf+*n,completion+len,add);*n+=add;buf[*n]=0;fwrite(completion+len,1,add,stdout);fflush(stdout);free(completion);return 1;}
static int ReadInteractiveLine(char *buf,size_t cap){
 struct termios old,raw;if(!isatty(STDIN_FILENO)||tcgetattr(STDIN_FILENO,&old)<0)return fgets(buf,cap,stdin)?(int)strlen(buf):-1;raw=old;raw.c_lflag&=~(ICANON|ECHO);raw.c_cc[VMIN]=1;raw.c_cc[VTIME]=0;if(tcsetattr(STDIN_FILENO,TCSAFLUSH,&raw)<0)return fgets(buf,cap,stdin)?(int)strlen(buf):-1;
 size_t n=0;for(;;){unsigned char c;if(read(STDIN_FILENO,&c,1)!=1){tcsetattr(STDIN_FILENO,TCSAFLUSH,&old);return -1;}if(c=='\r'||c=='\n'){putchar('\n');buf[n]=0;tcsetattr(STDIN_FILENO,TCSAFLUSH,&old);buf[n++]='\n';buf[n]=0;return (int)n;}if(c==3){putchar('^');putchar('C');putchar('\n');n=0;buf[0]=0;tcsetattr(STDIN_FILENO,TCSAFLUSH,&old);return 0;}if(c==127||c=='\b'){if(n){n--;printf("\b \b");fflush(stdout);}continue;}if(c=='\t'){CompleteLine(buf,&n,cap);continue;}if(isprint(c)&&n+1<cap){buf[n++]=(char)c;putchar(c);fflush(stdout);}}
}
static int Interactive(Runtime*r){char*buf=NULL;size_t n=0,cap=0;int depth=0;for(;;){printf(depth?"> ":"cs> ");fflush(stdout);char line[4096];int m=ReadInteractiveLine(line,sizeof line);if(m<0){putchar('\n');break;}if(m==0)continue;size_t lm=(size_t)m;if(n+lm+1>cap){cap=(n+lm+1)*2;buf=realloc(buf,cap);}memcpy(buf+n,line,lm);n+=lm;buf[n]=0;for(size_t k=n-lm;k<n;k++){if(line[k-(n-lm)]=='{')depth++;else if(line[k-(n-lm)]=='}'&&depth>0)depth--;}if(depth||line[lm-1]!='\n')continue;HistoryPush(&r->history,buf);RunText(r,buf);n=0;if(r->flow==FLOW_EXIT){free(buf);return r->returnStatus;}}free(buf);return r->status;}
extern char **environ;
static void ImportEnvironment(Runtime*r){for(char **e=environ;e&&*e;e++){char *eq=strchr(*e,'=');if(!eq||eq==*e)continue;char *name=DupN(*e,(size_t)(eq-*e));if(isalpha((unsigned char)name[0])||name[0]=='_'){bool valid=true;for(size_t i=1;name[i];i++)if(!isalnum((unsigned char)name[i])&&name[i]!='_'){valid=false;break;}if(valid)VarSetRaw(&r->vars,name,eq+1);}free(name);}}
int main(int argc,char**argv){signal(SIGINT,SigInt);Runtime r={0};r.jobs.next_id=1;r.shell_pgid=getpgrp();r.job_control=argc==1&&isatty(STDIN_FILENO)&&isatty(STDOUT_FILENO);if(r.job_control){setpgid(0,0);r.shell_pgid=getpgrp();tcsetpgrp(STDIN_FILENO,r.shell_pgid);signal(SIGTSTP,SIG_IGN);signal(SIGTTIN,SIG_IGN);signal(SIGTTOU,SIG_IGN);}r.interactive=argc==1;ImportEnvironment(&r);const char*path=getenv("PATH");if(path)VarSetRaw(&r.vars,"PATH",path);VarSetRaw(&r.vars,"status","0");setenv("status","0",1);SetScriptArgs(&r,argc,argv);if(argc>1){FILE*f=fopen(argv[1],"rb");if(!f){fprintf(stderr,"cs: %s: %s\n",argv[1],strerror(errno));return 2;}char*s=ReadAll(f);fclose(f);int rc=RunText(&r,s);free(s);return rc;}return Interactive(&r);}
