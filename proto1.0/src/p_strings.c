/* Primitives package */

#include "copyright.h"
#include "config.h"

#include <sys/types.h>
#include <stdio.h>
#include <time.h>
#include <ctype.h>
#include "db.h"
#include "tune.h"
#include "inst.h"
#include "externs.h"
#include "match.h"
#include "interface.h"
#include "params.h"
#include "strings.h"
#include "interp.h"

extern struct inst *oper1, *oper2, *oper3, *oper4;
extern struct inst temp1, temp2, temp3;
extern int tmp, result;
extern dbref ref;
extern char buf[BUFFER_LEN];
char *pname;

/* FMTTOKEN defines the start of a variable formatting string insertion */
#define FMTTOKEN '%'

void
prim_fmtstring(PRIM_PROTOTYPE)
{
	int slen, scnt, tstop, tlen, tnum, i;
	int slrj, spad1, spad2, slen1, slen2, temp;
	char sstr[BUFFER_LEN], sfmt[255], hold[256], tbuf[BUFFER_LEN];

	CHECKOP(1);
	oper1 = POP();
	if (oper1->type != PROG_STRING)
		abort_interp("Top argument must be a string.");
	if (!oper1->data.string) {
		CLEAR(oper1);
		PushNullStr;
		return;
	}
	/* We now have the non-null format string, parse it */
	result = 0;					/* End of current string, must be smaller than BUFFER_LEN */
	tmp = 0;					/* Number of props to search for/found */
	slen = strlen(oper1->data.string->data);
	scnt = 0;
	tstop = 0;
	strcpy(sstr, oper1->data.string->data);
	CLEAR(oper1);
	while ((scnt < slen) && (result < BUFFER_LEN)) {
		CHECKOP(0);
		if (sstr[scnt] == FMTTOKEN) {
			if (sstr[scnt + 1] == FMTTOKEN) {
				buf[result++] = FMTTOKEN;
				scnt += 2;
			} else {
				scnt++;
				if ((sstr[scnt] == '-') || (sstr[scnt] == '|')) {
					if (sstr[scnt] == '-')
						slrj = 1;
					else
						slrj = 2;
					scnt++;
				} else {
					slrj = 0;
				}
				if ((sstr[scnt] == '+') || (sstr[scnt] == ' ')) {
					if (sstr[scnt] == '+')
						spad1 = 1;
					else
						spad1 = 2;
					scnt++;
				} else {
					spad1 = 0;
				}
				if (sstr[scnt] == '0') {
					scnt++;
					spad2 = 1;
				} else {
					spad2 = 0;
				}
				slen1 = atoi(&sstr[scnt]);
				if ((sstr[scnt] >= '0') && (sstr[scnt] <= '9')) {
					while ((sstr[scnt] >= '0') && (sstr[scnt] <= '9'))
						scnt++;
				} else {
					if (sstr[scnt] == '*') {
						scnt++;
						CHECKOP(1);
						oper2 = POP();
						if (oper2->type != PROG_INTEGER)
							abort_interp("Format specified integer argument not found.");
						slen1 = oper2->data.number;
						CLEAR(oper2);
					} else {
						slen1 = -1;
					}
				}
				if (sstr[scnt] == '.') {
					scnt++;
					slen2 = atoi(&sstr[scnt]);
					if ((sstr[scnt] >= '0') && (sstr[scnt] <= '9')) {
						while ((sstr[scnt] >= '0') && (sstr[scnt] <= '9'))
							scnt++;
					} else {
						if (sstr[scnt] == '*') {
							scnt++;
							CHECKOP(1);
							oper2 = POP();
							if (oper2->type != PROG_INTEGER)
								abort_interp("Format specified integer argument not found.");
							slen2 = oper2->data.number;
							CLEAR(oper2);
						} else {
							abort_interp("Invalid format string.");
						}
					}
				} else {
					slen2 = -1;
				}
				if ((slen1 > 0) && ((slen1 + result) > BUFFER_LEN))
					abort_interp("Specified format field width too large.");
				sfmt[0] = '%';
				sfmt[1] = '\0';
				if (slrj == 1)
					strcat(sfmt, "-");
				if (spad1)
					if (spad1 == 1)
						strcat(sfmt, "+");
					else
						strcat(sfmt, " ");
				if (spad2)
					strcat(sfmt, "0");
				if (slen1 != -1) {
					sprintf(tbuf, "%d", slen1);
					strcat(sfmt, tbuf);
				}
				if (slen2 != -1) {
					sprintf(tbuf, ".%d", slen2);
					strcat(sfmt, tbuf);
				}
				CHECKOP(1);
				oper2 = POP();
				if (sstr[scnt] == '~') {
					switch (oper2->type) {
					case PROG_OBJECT:
						sstr[scnt] = 'D';
						break;
					case PROG_FLOAT:
						sstr[scnt] = 'g';
						break;
					case PROG_INTEGER:
						sstr[scnt] = 'i';
						break;
					case PROG_LOCK:
						sstr[scnt] = 'l';
						break;
					case PROG_STRING:
						sstr[scnt] = 's';
						break;
					default:
						sstr[scnt] = '?';
						break;
					}
				}
				switch (sstr[scnt]) {
				case 'i':
					strcat(sfmt, "d");
					if (oper2->type != PROG_INTEGER)
						abort_interp("Format specified integer argument not found.");
					sprintf(tbuf, sfmt, oper2->data.number);
					tlen = strlen(tbuf);
					if (slrj == 2) {
						tnum = 0;
						while ((tbuf[tnum] == ' ') && (tnum < tlen))
							tnum++;
						if ((tnum) && (tnum < tlen)) {
							temp = tnum / 2;
							for (i = tnum; i < tlen; i++)
								tbuf[i - temp] = tbuf[i];
							for (i = tlen - temp; i < tlen; i++)
								tbuf[i] = ' ';
						}
					}
					if (tlen + result > BUFFER_LEN)
						abort_interp("Resultant string would overflow buffer.");
					buf[result] = '\0';
					strcat(buf, tbuf);
					result += tlen;
					CLEAR(oper2);
					break;
				case 's':
					strcat(sfmt, "s");
					if (oper2->type != PROG_STRING)
						abort_interp("Format specified string argument not found.");
					sprintf(tbuf, sfmt,
							((oper2->data.string) ? oper2->data.string->data : ""));
					tlen = strlen(tbuf);
					if (slrj == 2) {
						tnum = 0;
						while ((tbuf[tnum] == ' ') && (tnum < tlen))
							tnum++;
						if ((tnum) && (tnum < tlen)) {
							temp = tnum / 2;
							for (i = tnum; i < tlen; i++)
								tbuf[i - temp] = tbuf[i];
							for (i = tlen - temp; i < tlen; i++)
								tbuf[i] = ' ';
						}
					}
					if (strlen(tbuf) + result > BUFFER_LEN)
						abort_interp("Resultant string would overflow buffer.");
					buf[result] = '\0';
					strcat(buf, tbuf);
					result += strlen(tbuf);
					CLEAR(oper2);
					break;
				case '?':
					strcat(sfmt, "s");
					switch (oper2->type) {
					case PROG_OBJECT:
						strcpy(hold, "OBJECT");
						break;
					case PROG_FLOAT:
						strcpy(hold, "FLOAT");
						break;
					case PROG_INTEGER:
						strcpy(hold, "INTEGER");
						break;
					case PROG_LOCK:
						strcpy(hold, "LOCK");
						break;
					case PROG_STRING:
						strcpy(hold, "STRING");
						break;
					case PROG_VAR:
						strcpy(hold, "VARIABLE");
						break;
					case PROG_LVAR:
						strcpy(hold, "LOCAL-VARIABLE");
						break;
					case PROG_ADD:
						strcpy(hold, "ADDRESS");
						break;
					case PROG_ARRAY:
						strcpy(hold, "ARRAY");
						break;
					case PROG_FUNCTION:
						strcpy(hold, "FUNCTION-NAME");
						break;
					case PROG_IF:
						strcpy(hold, "IF-STATEMENT");
						break;
					case PROG_EXEC:
						strcpy(hold, "EXECUTE");
						break;
					case PROG_JMP:
						strcpy(hold, "JUMP");
						break;
					case PROG_PRIMITIVE:
						strcpy(hold, "PRIMITIVE");
						break;
					default:
						strcpy(hold, "UNKNOWN");
					}
					sprintf(tbuf, sfmt, hold);
					tlen = strlen(tbuf);
					if (slrj == 2) {
						tnum = 0;
						while ((tbuf[tnum] == ' ') && (tnum < tlen))
							tnum++;
						if ((tnum) && (tnum < tlen)) {
							temp = tnum / 2;
							for (i = tnum; i < tlen; i++)
								tbuf[i - temp] = tbuf[i];
							for (i = tlen - temp; i < tlen; i++)
								tbuf[i] = ' ';
						}
					}
					if (strlen(tbuf) + result > BUFFER_LEN)
						abort_interp("Resultant string would overflow buffer.");
					buf[result] = '\0';
					strcat(buf, tbuf);
					result += strlen(tbuf);
					CLEAR(oper2);
					break;
				case 'd':
					strcat(sfmt, "s");
					if (oper2->type != PROG_OBJECT)
						abort_interp("Format specified object not found.");
					sprintf(hold, "#%d", oper2->data.objref);
					sprintf(tbuf, sfmt, hold);
					tlen = strlen(tbuf);
					if (slrj == 2) {
						tnum = 0;
						while ((tbuf[tnum] == ' ') && (tnum < tlen))
							tnum++;
						if ((tnum) && (tnum < tlen)) {
							temp = tnum / 2;
							for (i = tnum; i < tlen; i++)
								tbuf[i - temp] = tbuf[i];
							for (i = tlen - temp; i < tlen; i++)
								tbuf[i] = ' ';
						}
					}
					if (strlen(tbuf) + result > BUFFER_LEN)
						abort_interp("Resultant string would overflow buffer.");
					buf[result] = '\0';
					strcat(buf, tbuf);
					result += strlen(tbuf);
					CLEAR(oper2);
					break;
				case 'D':
					strcat(sfmt, "s");
					if (oper2->type != PROG_OBJECT)
						abort_interp("Format specified object not found.");
					if (!valid_object(oper2))
						abort_interp("Format specified object not valid.");
					ref = oper2->data.objref;
					CHECKREMOTE(ref);
					if ((Typeof(ref) != TYPE_PLAYER) && (Typeof(ref) != TYPE_PROGRAM))
						ts_lastuseobject(ref);
					if (NAME(ref)) {
						strcpy(hold, PNAME(ref));
					} else {
						hold[0] = '\0';
					}
					sprintf(tbuf, sfmt, hold);
					tlen = strlen(tbuf);
					if (slrj == 2) {
						tnum = 0;
						while ((tbuf[tnum] == ' ') && (tnum < tlen))
							tnum++;
						if ((tnum) && (tnum < tlen)) {
							temp = tnum / 2;
							for (i = tnum; i < tlen; i++)
								tbuf[i - temp] = tbuf[i];
							for (i = tlen - temp; i < tlen; i++)
								tbuf[i] = ' ';
						}
					}
					if (strlen(tbuf) + result > BUFFER_LEN)
						abort_interp("Resultant string would overflow buffer.");
					buf[result] = '\0';
					strcat(buf, tbuf);
					result += strlen(tbuf);
					CLEAR(oper2);
					break;
				case 'l':
					strcat(sfmt, "s");
					if (oper2->type != PROG_LOCK)
						abort_interp("Format specified lock not found.");
					strcpy(hold, unparse_boolexp(ProgUID, oper2->data.lock, 1));
					sprintf(tbuf, sfmt, hold);
					tlen = strlen(tbuf);
					if (slrj == 2) {
						tnum = 0;
						while ((tbuf[tnum] == ' ') && (tnum < tlen))
							tnum++;
						if ((tnum) && (tnum < tlen)) {
							temp = tnum / 2;
							for (i = tnum; i < tlen; i++)
								tbuf[i - temp] = tbuf[i];
							for (i = tlen - temp; i < tlen; i++)
								tbuf[i] = ' ';
						}
					}
					if (strlen(tbuf) + result > BUFFER_LEN)
						abort_interp("Resultant string would overflow buffer.");
					buf[result] = '\0';
					strcat(buf, tbuf);
					result += strlen(tbuf);
					CLEAR(oper2);
					break;
				case 'f':
				case 'e':
				case 'g':
					strcat(sfmt, "h");
					sprintf(hold, "%c", sstr[scnt]);
					strcat(sfmt, hold);
					if (oper2->type != PROG_FLOAT)
						abort_interp("Format specified float not found.");
					sprintf(tbuf, sfmt, oper2->data.fnumber);
					tlen = strlen(tbuf);
					if (slrj == 2) {
						tnum = 0;
						while ((tbuf[tnum] == ' ') && (tnum < tlen))
							tnum++;
						if ((tnum) && (tnum < tlen)) {
							temp = tnum / 2;
							for (i = tnum; i < tlen; i++)
								tbuf[i - temp] = tbuf[i];
							for (i = tlen - temp; i < tlen; i++)
								tbuf[i] = ' ';
						}
					}
					if (strlen(tbuf) + result > BUFFER_LEN)
						abort_interp("Resultant string would overflow buffer.");
					buf[result] = '\0';
					strcat(buf, tbuf);
					result += strlen(tbuf);
					CLEAR(oper2);
					break;
				default:
					abort_interp("Invalid format string.");
					break;
				}
				scnt++;
				tstop += strlen(tbuf);
			}
		} else {
			if ((sstr[scnt] == '\\') && (sstr[scnt + 1] == 't')) {
				if ((result - (tstop % 8) + 1) >= BUFFER_LEN)
					abort_interp("Resultant string would overflow buffer.");
				if ((tstop % 8) == 0) {
					buf[result++] = ' ';
					tstop++;
				}
				while ((tstop % 8) != 0) {
					buf[result++] = ' ';
					tstop++;
				}
				scnt += 2;
				tstop = 0;
			} else {
				if (sstr[scnt] == '\r') {
					tstop = 0;
					scnt++;
					buf[result++] = '\r';
				} else {
					buf[result++] = sstr[scnt++];
					tstop++;
				}
			}
		}
	}
	CHECKOP(0);
	if (result >= BUFFER_LEN)
		abort_interp("Resultant string would overflow buffer.");
	buf[result] = '\0';
	if (result)
		PushString(buf);
	else
		PushNullStr;
}

void
prim_split(PRIM_PROTOTYPE)
{
	char *temp;

	CHECKOP(2);
	oper1 = POP();
	oper2 = POP();
	if (oper1->type != PROG_STRING)
		abort_interp("Non-string argument. (2)");
	if (!oper1->data.string)
		abort_interp("Null split argument. (2)");
	if (oper2->type != PROG_STRING)
		abort_interp("Non-string argument. (1)");
	if (!oper2->data.string) {
		result = 0;
	} else {
		strcpy(buf, oper2->data.string->data);
		pname = strstr(buf, oper1->data.string->data);
		if (!pname) {
			result = -1;
		} else {
			temp = pname + oper1->data.string->length;
			*pname = '\0';
			result = 1;
		}
	}
	CLEAR(oper1);
	CLEAR(oper2);
	if (result) {
		if (result == 1) {
			if (buf[0] == '\0') {
				PushNullStr;
			} else {
				PushString(buf);
			}
			if (temp[0] == '\0') {
				PushNullStr;
			} else {
				PushString(temp);
			}
		} else {
			PushString(buf);
			PushNullStr;
		}
	} else {
		PushNullStr;
		PushNullStr;
	}
}

void
prim_rsplit(PRIM_PROTOTYPE)
{
	char *temp, *hold;

	CHECKOP(2);
	oper1 = POP();
	oper2 = POP();
	if (oper1->type != PROG_STRING)
		abort_interp("Non-string argument. (2)");
	if (!oper1->data.string)
		abort_interp("Null split argument. (2)");
	if (oper2->type != PROG_STRING)
		abort_interp("Non-string argument. (1)");
	if (!oper2->data.string) {
		result = 0;
	} else {
		if (oper1->data.string->length > oper2->data.string->length) {
			result = -1;
		} else {
			strcpy(buf, oper2->data.string->data);
			temp = buf + (oper2->data.string->length - oper1->data.string->length);
			hold = NULL;
			while ((temp != (buf - 1)) && (!hold)) {
				if (*temp == *(oper1->data.string->data))
					if (!strncmp(temp, oper1->data.string->data, oper1->data.string->length))
						hold = temp;
				temp--;
			}
			if (!hold) {
				result = -1;
			} else {
				*hold = '\0';
				hold += oper1->data.string->length;
				result = 1;
			}
		}
	}
	CLEAR(oper1);
	CLEAR(oper2);
	if (result) {
		if (result == 1) {
			if (buf[0] == '\0') {
				PushNullStr;
			} else {
				PushString(buf);
			}
			if (hold[0] == '\0') {
				PushNullStr;
			} else {
				PushString(hold);
			}
		} else {
			PushString(buf);
			PushNullStr;
		}
	} else {
		PushNullStr;
		PushNullStr;
	}
}

void
prim_ctoi(PRIM_PROTOTYPE)
{
	char c;

	CHECKOP(1);
	oper1 = POP();
	if (oper1->type != PROG_STRING)
		abort_interp("Non-string argument. (1)");
	if (!oper1->data.string) {
		c = '\0';
	} else {
		c = oper1->data.string->data[0];
	}
	result = c;
	CLEAR(oper1);
	PushInt(result);
}

void
prim_itoc(PRIM_PROTOTYPE)
{
	CHECKOP(1);
	oper1 = POP();
	if ((oper1->type != PROG_INTEGER) || (oper1->data.number < 0))
		abort_interp("Argument must be a positive integer. (1)");
	if (oper1->data.number > 127 || (!isprint((char) oper1->data.number) &&
									 ((char) oper1->data.number != '\r') &&
									 ((char) oper1->data.number != '\['))) {
		result = 0;
	} else {
		result = 1;
		buf[0] = (char) oper1->data.number;
		buf[1] = '\0';
	}
	CLEAR(oper1);
	if (result) {
		PushString(buf);
	} else {
		PushNullStr;
	}
}

void
prim_stod(PRIM_PROTOTYPE)
{
	CHECKOP(1);
	oper1 = POP();
	if (oper1->type != PROG_STRING)
		abort_interp("Non-string argument. (1)");
	if (!oper1->data.string) {
		ref = NOTHING;
	} else {
		tmp = 0;
		while (oper1->data.string->data[tmp] == ' ')
			tmp++;
		if (oper1->data.string->data[tmp] == '#') {
			if (isdigit(oper1->data.string->data[tmp + 1])) {
				result = atoi(&oper1->data.string->data[tmp + 1]);
				if ((!result) && (oper1->data.string->data[tmp + 1] != '0')) {
					ref = NOTHING;
				} else {
					ref = (dbref) result;
				}
			} else {
				ref = NOTHING;
			}
		} else {
			result = atoi(&oper1->data.string->data[tmp]);
			if ((!result) && (oper1->data.string->data[tmp] != '0')) {
				ref = NOTHING;
			} else {
				ref = (dbref) result;
			}
		}
	}
	CLEAR(oper1);
	PushObject(ref);
}

void
prim_dtos(PRIM_PROTOTYPE)
{
   CHECKOP(1);
   oper1 = POP();
   if(oper1->type != PROG_OBJECT)
      abort_interp("No Object Dbref was passed as an argment.");
   sprintf(buf, "#%d", oper1->data.objref);
   PushString(buf);
}


void
prim_midstr(PRIM_PROTOTYPE)
{
	int start, range;

	CHECKOP(3);
	oper1 = POP();
	oper2 = POP();
	oper3 = POP();
	result = 0;
	if (oper3->type != PROG_STRING)
		abort_interp("Non-string argument. (1)");
	if (oper2->type != PROG_INTEGER)
		abort_interp("Non-integer argument. (2)");
	if (oper2->data.number < 1)
		abort_interp("Data must be a positive integer. (2)");
	if (oper1->type != PROG_INTEGER)
		abort_interp("Non-integer argument. (3)");
	if (oper1->data.number < 0)
		abort_interp("Data must be a positive integer. (3)");
	if (!oper3->data.string) {
		result = 1;
	} else {
		if (oper2->data.number > oper3->data.string->length) {
			result = 1;
		} else {
			start = oper2->data.number - 1;
			if ((oper1->data.number + start) > oper3->data.string->length) {
				range = oper3->data.string->length - start;
			} else {
				range = oper1->data.number;
			}
			bcopy(oper3->data.string->data + start, buf, range);
			buf[range] = '\0';
		}
	}
	CLEAR(oper1);
	CLEAR(oper2);
	CLEAR(oper3);
	if (result) {
		PushNullStr;
	} else {
		PushString(buf);
	}
}

void 
prim_numberp(PRIM_PROTOTYPE)
{
    CHECKOP(1);
    oper1 = POP();
    if (oper1->type != PROG_STRING || !oper1->data.string)
	result = 0;
    else
	result = number(oper1->data.string->data);
    CLEAR(oper1);
    PushInt(result);
}

void 
prim_stringcmp(PRIM_PROTOTYPE)
{
    CHECKOP(2);
    oper1 = POP();
    oper2 = POP();
    if (oper1->type != PROG_STRING || oper2->type != PROG_STRING)
	abort_interp("Non-string argument.");
    if (oper1->data.string == oper2->data.string)
	result = 0;
    else if (!(oper2->data.string && oper1->data.string))
	result = oper1->data.string ? -1 : 1;
    else {
	result = string_compare(oper2->data.string->data, oper1->data.string->data);
    }
    CLEAR(oper1);
    CLEAR(oper2);
    PushInt(result);
}

void 
prim_strcmp(PRIM_PROTOTYPE)
{
    CHECKOP(2);
    oper1 = POP();
    oper2 = POP();
    if (oper1->type != PROG_STRING || oper2->type != PROG_STRING)
	abort_interp("Non-string argument.");
    if (oper1->data.string == oper2->data.string)
	result = 0;
    else if (!(oper2->data.string && oper1->data.string))
	result = oper1->data.string ? -1 : 1;
    else {
	result = strcmp(oper2->data.string->data, oper1->data.string->data);
    }
    CLEAR(oper1);
    CLEAR(oper2);
    PushInt(result);
}

void 
prim_strncmp(PRIM_PROTOTYPE)
{
    CHECKOP(3);
    oper1 = POP();
    oper2 = POP();
    oper3 = POP();
    if (oper1->type != PROG_INTEGER)
	abort_interp("Non-integer argument.");
    if (oper2->type != PROG_STRING || oper3->type != PROG_STRING)
	abort_interp("Non-string argument.");
    if (oper2->data.string == oper3->data.string)
	result = 0;
    else if (!(oper3->data.string && oper2->data.string))
	result = oper2->data.string ? -1 : 1;
    else
	result = strncmp(oper3->data.string->data, oper2->data.string->data,
			 oper1->data.number);
    CLEAR(oper1);
    CLEAR(oper2);
    CLEAR(oper3);
    PushInt(result);
}

void 
prim_strcut(PRIM_PROTOTYPE)
{
    CHECKOP(2);
    temp1 = *(oper1 = POP());
    temp2 = *(oper2 = POP());
    if (temp1.type != PROG_INTEGER)
	abort_interp("Non-integer argument (2)");
    if (temp1.data.number < 0)
	abort_interp("Argument must be a positive integer.");
    if (temp2.type != PROG_STRING)
	abort_interp("Non-string argument (1)");
    if (!temp2.data.string) {
	PushNullStr;
	PushNullStr;
    } else {
	if (temp1.data.number > temp2.data.string->length) {
	    temp2.data.string->links++;
	    PushStrRaw(temp2.data.string);
	    PushNullStr;
	} else {
	    bcopy(temp2.data.string->data, buf, temp1.data.number);
	    buf[temp1.data.number] = '\0';
	    PushString(buf);
	    if (temp2.data.string->length > temp1.data.number) {
		bcopy(temp2.data.string->data + temp1.data.number, buf,
		      temp2.data.string->length - temp1.data.number + 1);
		PushString(buf);
	    } else {
		PushNullStr;
	    }
	}
    }
    CLEAR(&temp1);
    CLEAR(&temp2);
}

void 
prim_strlen(PRIM_PROTOTYPE)
{
    CHECKOP(1);
    oper1 = POP();
    if (oper1->type != PROG_STRING)
	abort_interp("Non-string argument.");
    if (!oper1->data.string)
	result = 0;
    else
	result = oper1->data.string->length;
    CLEAR(oper1);
    PushInt(result);
}

void 
prim_strcat(PRIM_PROTOTYPE)
{
    struct shared_string *string;

    CHECKOP(2);
    oper1 = POP();
    oper2 = POP();
    if (oper1->type != PROG_STRING || oper2->type != PROG_STRING)
	abort_interp("Non-string argument.");
    if (!oper1->data.string && !oper2->data.string)
	string = NULL;
    else if (!oper2->data.string) {
	oper1->data.string->links++;
	string = oper1->data.string;
    } else if (!oper1->data.string) {
	oper2->data.string->links++;
	string = oper2->data.string;
    } else if (oper1->data.string->length + oper2->data.string->length
	       > (BUFFER_LEN) - 1) {
	abort_interp("Operation would result in overflow.");
    } else {
	bcopy(oper2->data.string->data, buf, oper2->data.string->length);
	bcopy(oper1->data.string->data, buf + oper2->data.string->length,
	      oper1->data.string->length + 1);
	string = alloc_prog_string(buf);
    }
    CLEAR(oper1);
    CLEAR(oper2);
    PushStrRaw(string);
}

void 
prim_atoi(PRIM_PROTOTYPE)
{
    CHECKOP(1);
    oper1 = POP();
    if (oper1->type != PROG_STRING || !oper1->data.string)
	result = 0;
    else
	result = atoi(oper1->data.string->data);
    CLEAR(oper1);
    PushInt(result);
}

void 
prim_notify_descriptor(PRIM_PROTOTYPE)
{
    CHECKOP(2);
    oper1 = POP();
    oper2 = POP();
    if (mlev < LMAGE)
        abort_interp("Mage primitive.");
    if (oper1->type != PROG_STRING)
	abort_interp("Non-string argument (2)");
    if (oper2->type != PROG_INTEGER)
	abort_interp("Invalid object argument (1)");
    if (oper1->data.string)
	strcpy(buf, oper1->data.string->data);
        notify_descriptor(oper2->data.number, buf);
    CLEAR(oper1);
    CLEAR(oper2);
}

void 
prim_notify(PRIM_PROTOTYPE)
{
    struct inst *oper1, *oper2;
    char buf2[BUFFER_LEN*2];
    CHECKOP(2);
    oper1 = POP();
    oper2 = POP();
    if (oper1->type != PROG_STRING)
	abort_interp("Non-string argument (2)");
    if (!valid_object(oper2))
	abort_interp("Invalid object argument (1)");
    CHECKREMOTE(oper2->data.objref);

    if (oper1->data.string) {
	strcpy(buf, oper1->data.string->data);
	if (tp_m1_name_notify &&
		mlev < LM2  && player != oper2->data.objref) {
	    strcpy(buf2, PNAME(player));
	    strcat(buf2, " ");
	    if (!string_prefix(buf, buf2)) {
		strcat(buf2, buf);
		buf2[BUFFER_LEN-1] = '\0';
		strcpy(buf, buf2);
	    }
	}
	notify_listeners(player, program, oper2->data.objref,
			 getloc(oper2->data.objref), buf, 1);
    }
    CLEAR(oper1);
    CLEAR(oper2);
}

void 
prim_notify_html(PRIM_PROTOTYPE)
{
    struct inst *oper1, *oper2;
    char buf2[BUFFER_LEN*2];
    CHECKOP(2);
    oper1 = POP();
    oper2 = POP();
    if (oper1->type != PROG_STRING)
	abort_interp("Non-string argument (2)");
    if (!valid_object(oper2))
	abort_interp("Invalid object argument (1)");
    CHECKREMOTE(oper2->data.objref);

    if (oper1->data.string) {
	strcpy(buf, oper1->data.string->data);
	if (tp_m1_name_notify &&
		mlev < LM2  && player != oper2->data.objref) {
	    strcpy(buf2, PNAME(player));
	    strcat(buf2, " ");
	    if (!string_prefix(buf, buf2)) {
		strcat(buf2, buf);
		buf2[BUFFER_LEN-1] = '\0';
		strcpy(buf, buf2);
	    }
	}
        strcat(buf, "\r");
	notify_html_listeners(player, program, oper2->data.objref,
			 getloc(oper2->data.objref), buf, 1);
    }
    CLEAR(oper1);
    CLEAR(oper2);
}

void 
prim_notify_html_nocr(PRIM_PROTOTYPE)
{
    struct inst *oper1, *oper2;
    char buf2[BUFFER_LEN*2];
    CHECKOP(2);
    oper1 = POP();
    oper2 = POP();
    if (oper1->type != PROG_STRING)
	abort_interp("Non-string argument (2)");
    if (!valid_object(oper2))
	abort_interp("Invalid object argument (1)");
    CHECKREMOTE(oper2->data.objref);

    if (oper1->data.string) {
	strcpy(buf, oper1->data.string->data);
	if (tp_m1_name_notify &&
		mlev < LM2  && player != oper2->data.objref) {
	    strcpy(buf2, PNAME(player));
	    strcat(buf2, " ");
	    if (!string_prefix(buf, buf2)) {
		strcat(buf2, buf);
		buf2[BUFFER_LEN-1] = '\0';
		strcpy(buf, buf2);
	    }
	}
	notify_html_listeners(player, program, oper2->data.objref,
			 getloc(oper2->data.objref), buf, 1);
    }
    CLEAR(oper1);
    CLEAR(oper2);
}

void 
prim_ansi_notify(PRIM_PROTOTYPE)
{
    char buf2[BUFFER_LEN*2];
    CHECKOP(2);
    oper1 = POP();
    oper2 = POP();
    if (oper1->type != PROG_STRING)
	abort_interp("Non-string argument (2)");
    if (!valid_object(oper2))
	abort_interp("Invalid object argument (1)");
    CHECKREMOTE(oper2->data.objref);

    if (oper1->data.string) {
	strcpy(buf, oper1->data.string->data);
	if (tp_m1_name_notify &&
		mlev < LM2  && player != oper2->data.objref) {
	    strcpy(buf2, PNAME(player));
	    strcat(buf2, " ");
	    if (!string_prefix(buf, buf2)) {
		strcat(buf2, buf);
		buf2[BUFFER_LEN-1] = '\0';
		strcpy(buf, buf2);
	    }
	}
	ansi_notify_listeners(player, program, oper2->data.objref,
			 getloc(oper2->data.objref), buf, 1);
    }
    CLEAR(oper1);
    CLEAR(oper2);
}


void 
prim_notify_exclude(PRIM_PROTOTYPE)
{
    /* roomD excludeDn ... excludeD1 nI messageS  -- */
    struct inst *oper1, *oper2;
    char buf2[BUFFER_LEN*2];
    CHECKOP(2);
    oper1 = POP();
    oper2 = POP();
    if (oper2->type != PROG_INTEGER)
	abort_interp("non-integer count argument (top-1)");
    if (oper1->type != PROG_STRING)
	abort_interp("Non-string message argument (top)");
    strcpy(buf, DoNullInd(oper1->data.string));

    if (tp_m1_name_notify && mlev < LM2) {
	strcpy(buf2, PNAME(player));
	strcat(buf2, " ");
	if (!string_prefix(buf, buf2)) {
	    strcat(buf2, buf);
	    buf2[BUFFER_LEN-1] = '\0';
	    strcpy(buf, buf2);
	}
    }

    result = oper2->data.number;
    CLEAR(oper1);
    CLEAR(oper2);
    {
	dbref   what, where, excluded[STACK_SIZE];
	int     count, i;

	CHECKOP(0);
	count = i = result;
	if (i >= STACK_SIZE || i < 0)
	    abort_interp("Count argument is out of range.");
	while (i > 0) {
	    CHECKOP(1);
	    oper1 = POP();
	    if (oper1->type != PROG_OBJECT)
		abort_interp("Invalid object argument.");
	    excluded[--i] = oper1->data.objref;
	    CLEAR(oper1);
	}
	CHECKOP(1);
	oper1 = POP();
	if (!valid_object(oper1))
	    abort_interp("Non-object argument (1)");
	where = oper1->data.objref;
	if (Typeof(where) != TYPE_ROOM && Typeof(where) != TYPE_THING &&
		Typeof(where) != TYPE_PLAYER)
	    abort_interp("Invalid location argument (1)");
	CHECKREMOTE(where);
	what = DBFETCH(where)->contents;
	CLEAR(oper1);
	if (*buf) {
	    while (what != NOTHING) {
		if (Typeof(what) != TYPE_ROOM) {
		    for (tmp = 0, i = count; i-- > 0;) {
			if (excluded[i] == what)
			    tmp = 1;
		    }
		} else {
		    tmp = 1;
		}
		if (!tmp)
		    notify_listeners(player, program, what, where, buf, 0);
		what = DBFETCH(what)->next;
	    }
	}

	if (tp_listeners) {
	    notify_listeners(player, program, where, where, buf, 0);
	    if (tp_listeners_env) {
		what = DBFETCH(where)->location;
		for (; what != NOTHING; what = DBFETCH(what)->location)
		    notify_listeners(player, program, what, where, buf, 0);
	    }
	}
    }
}



void 
prim_ansi_notify_exclude(PRIM_PROTOTYPE)
{
    /* roomD excludeDn ... excludeD1 nI messageS  -- */
    struct inst *oper1, *oper2;
    char buf2[BUFFER_LEN*2];
    CHECKOP(2);
    oper1 = POP();
    oper2 = POP();
    if (oper2->type != PROG_INTEGER)
	abort_interp("non-integer count argument (top-1)");
    if (oper1->type != PROG_STRING)
	abort_interp("Non-string message argument (top)");
    strcpy(buf, DoNullInd(oper1->data.string));

    if (tp_m1_name_notify && mlev < LM2) {
	strcpy(buf2, PNAME(player));
	strcat(buf2, " ");
	if (!string_prefix(buf, buf2)) {
	    strcat(buf2, buf);
	    buf2[BUFFER_LEN-1] = '\0';
	    strcpy(buf, buf2);
	}
    }

    result = oper2->data.number;
    CLEAR(oper1);
    CLEAR(oper2);
    {
	dbref   what, where, excluded[STACK_SIZE];
	int     count, i;

	CHECKOP(0);
	count = i = result;
	if (i >= STACK_SIZE || i < 0)
	    abort_interp("Count argument is out of range.");
	while (i > 0) {
	    CHECKOP(1);
	    oper1 = POP();
	    if (oper1->type != PROG_OBJECT)
		abort_interp("Invalid object argument.");
	    excluded[--i] = oper1->data.objref;
	    CLEAR(oper1);
	}
	CHECKOP(1);
	oper1 = POP();
	if (!valid_object(oper1))
	    abort_interp("Non-object argument (1)");
	where = oper1->data.objref;
	if (Typeof(where) != TYPE_ROOM && Typeof(where) != TYPE_THING &&
		Typeof(where) != TYPE_PLAYER)
	    abort_interp("Invalid location argument (1)");
	CHECKREMOTE(where);
	what = DBFETCH(where)->contents;
	CLEAR(oper1);
	if (*buf) {
	    while (what != NOTHING) {
		if (Typeof(what) != TYPE_ROOM) {
		    for (tmp = 0, i = count; i-- > 0;) {
			if (excluded[i] == what)
			    tmp = 1;
		    }
		} else {
		    tmp = 1;
		}
		if (!tmp)
		    ansi_notify_listeners(player, program, what, where, buf, 0);
		what = DBFETCH(what)->next;
	    }
	}

	if (tp_listeners) {
	    notify_listeners(player, program, where, where, buf, 0);
	    if (tp_listeners_env) {
		what = DBFETCH(where)->location;
		for (; what != NOTHING; what = DBFETCH(what)->location)
		    ansi_notify_listeners(player, program, what, where, buf, 0);
	    }
	}
    }
}


void 
prim_notify_html_exclude(PRIM_PROTOTYPE)
{
    /* roomD excludeDn ... excludeD1 nI messageS  -- */
    struct inst *oper1, *oper2;
    char buf2[BUFFER_LEN*2];
    CHECKOP(2);
    oper1 = POP();
    oper2 = POP();
    if (oper2->type != PROG_INTEGER)
	abort_interp("non-integer count argument (top-1)");
    if (oper1->type != PROG_STRING)
	abort_interp("Non-string message argument (top)");
    strcpy(buf, DoNullInd(oper1->data.string));

    if (tp_m1_name_notify && mlev < LM2) {
	strcpy(buf2, PNAME(player));
	strcat(buf2, " ");
	if (!string_prefix(buf, buf2)) {
	    strcat(buf2, buf);
	    buf2[BUFFER_LEN-1] = '\0';
	    strcpy(buf, buf2);
	}
    }
    strcat(buf, "\r");

    result = oper2->data.number;
    CLEAR(oper1);
    CLEAR(oper2);
    {
	dbref   what, where, excluded[STACK_SIZE];
	int     count, i;

	CHECKOP(0);
	count = i = result;
	if (i >= STACK_SIZE || i < 0)
	    abort_interp("Count argument is out of range.");
	while (i > 0) {
	    CHECKOP(1);
	    oper1 = POP();
	    if (oper1->type != PROG_OBJECT)
		abort_interp("Invalid object argument.");
	    excluded[--i] = oper1->data.objref;
	    CLEAR(oper1);
	}
	CHECKOP(1);
	oper1 = POP();
	if (!valid_object(oper1))
	    abort_interp("Non-object argument (1)");
	where = oper1->data.objref;
	if (Typeof(where) != TYPE_ROOM && Typeof(where) != TYPE_THING &&
		Typeof(where) != TYPE_PLAYER)
	    abort_interp("Invalid location argument (1)");
	CHECKREMOTE(where);
	what = DBFETCH(where)->contents;
	CLEAR(oper1);
	if (*buf) {
	    while (what != NOTHING) {
		if (Typeof(what) != TYPE_ROOM) {
		    for (tmp = 0, i = count; i-- > 0;) {
			if (excluded[i] == what)
			    tmp = 1;
		    }
		} else {
		    tmp = 1;
		}
		if (!tmp)
		    notify_html_listeners(player, program, what, where, buf, 0);
		what = DBFETCH(what)->next;
	    }
	}

	if (tp_listeners) {
	    notify_html_listeners(player, program, where, where, buf, 0);
	    if (tp_listeners_env) {
		what = DBFETCH(where)->location;
		for (; what != NOTHING; what = DBFETCH(what)->location)
		    notify_html_listeners(player, program, what, where, buf, 0);
	    }
	}
    }
}

void 
prim_notify_html_exclude_nocr(PRIM_PROTOTYPE)
{
    /* roomD excludeDn ... excludeD1 nI messageS  -- */
    struct inst *oper1, *oper2;
    char buf2[BUFFER_LEN*2];
    CHECKOP(2);
    oper1 = POP();
    oper2 = POP();
    if (oper2->type != PROG_INTEGER)
	abort_interp("non-integer count argument (top-1)");
    if (oper1->type != PROG_STRING)
	abort_interp("Non-string message argument (top)");
    strcpy(buf, DoNullInd(oper1->data.string));

    if (tp_m1_name_notify && mlev < LM2) {
	strcpy(buf2, PNAME(player));
	strcat(buf2, " ");
	if (!string_prefix(buf, buf2)) {
	    strcat(buf2, buf);
	    buf2[BUFFER_LEN-1] = '\0';
	    strcpy(buf, buf2);
	}
    }

    result = oper2->data.number;
    CLEAR(oper1);
    CLEAR(oper2);
    {
	dbref   what, where, excluded[STACK_SIZE];
	int     count, i;

	CHECKOP(0);
	count = i = result;
	if (i >= STACK_SIZE || i < 0)
	    abort_interp("Count argument is out of range.");
	while (i > 0) {
	    CHECKOP(1);
	    oper1 = POP();
	    if (oper1->type != PROG_OBJECT)
		abort_interp("Invalid object argument.");
	    excluded[--i] = oper1->data.objref;
	    CLEAR(oper1);
	}
	CHECKOP(1);
	oper1 = POP();
	if (!valid_object(oper1))
	    abort_interp("Non-object argument (1)");
	where = oper1->data.objref;
	if (Typeof(where) != TYPE_ROOM && Typeof(where) != TYPE_THING &&
		Typeof(where) != TYPE_PLAYER)
	    abort_interp("Invalid location argument (1)");
	CHECKREMOTE(where);
	what = DBFETCH(where)->contents;
	CLEAR(oper1);
	if (*buf) {
	    while (what != NOTHING) {
		if (Typeof(what) != TYPE_ROOM) {
		    for (tmp = 0, i = count; i-- > 0;) {
			if (excluded[i] == what)
			    tmp = 1;
		    }
		} else {
		    tmp = 1;
		}
		if (!tmp)
		    notify_html_listeners(player, program, what, where, buf, 0);
		what = DBFETCH(what)->next;
	    }
	}

	if (tp_listeners) {
	    notify_html_listeners(player, program, where, where, buf, 0);
	    if (tp_listeners_env) {
		what = DBFETCH(where)->location;
		for (; what != NOTHING; what = DBFETCH(what)->location)
		    notify_html_listeners(player, program, what, where, buf, 0);
	    }
	}
    }
}

void 
prim_intostr(PRIM_PROTOTYPE)
{
    CHECKOP(1);
    oper1 = POP();
    if (oper1->type == PROG_STRING)
	abort_interp("Invalid argument.");
    sprintf(buf, "%d", oper1->data.number);
    CLEAR(oper1);
    PushString(buf);
}

void 
prim_explode(PRIM_PROTOTYPE)
{
    CHECKOP(2);
    temp1 = *(oper1 = POP());
    temp2 = *(oper2 = POP());
    oper1 = &temp1;
    oper2 = &temp2;
    if (temp1.type != PROG_STRING)
	abort_interp("Non-string argument (2)");
    if (temp2.type != PROG_STRING)
	abort_interp("Non-string argument (1)");
    if (!temp1.data.string)
	abort_interp("Empty string argument (2)");
    {
	int     i;
	const char *delimit = temp1.data.string->data;

	if (!temp2.data.string) {
	    result = 1;
	    CLEAR(&temp1);
	    CLEAR(&temp2);
	    PushNullStr;
	    PushInt(result);
	    return;
	} else {
	    result = 0;
	    bcopy(temp2.data.string->data, buf, temp2.data.string->length + 1);
	    for (i = temp2.data.string->length - 1; i >= 0; i--) {
		if (!strncmp(buf + i, delimit, temp1.data.string->length)) {
		    buf[i] = '\0';
		    CHECKOFLOW(1);
		    PushString((buf + i + temp1.data.string->length));
		    result++;
		}
	    }
	    CHECKOFLOW(1);
	    PushString(buf);
	    result++;
	}
    }
    CHECKOFLOW(1);
    CLEAR(&temp1);
    CLEAR(&temp2);
    PushInt(result);
}

void 
prim_subst(PRIM_PROTOTYPE)
{
    CHECKOP(3);
    oper1 = POP();
    oper2 = POP();
    oper3 = POP();
    if (oper1->type != PROG_STRING)
	abort_interp("Non-string argument (3)");
    if (oper2->type != PROG_STRING)
	abort_interp("Non-string argument (2)");
    if (oper3->type != PROG_STRING)
	abort_interp("Non-string argument (1)");
    if (!oper1->data.string)
	abort_interp("Empty string argument (3)");
    {
	int     i = 0, j = 0, k = 0;
	const char *match;
	const char *replacement;
	char    xbuf[BUFFER_LEN];

	buf[0] = '\0';
	if (oper3->data.string) {
	    bcopy(oper3->data.string->data, xbuf, oper3->data.string->length + 1);
	    match = oper1->data.string->data;
	    replacement = DoNullInd(oper2->data.string);
	    k = *replacement ? oper2->data.string->length : 0;
	    while (xbuf[i]) {
		if (!strncmp(xbuf + i, match, oper1->data.string->length)) {
		    if ((j + k + 1) > BUFFER_LEN)
			abort_interp("Operation would result in overflow.");
		    strcat(buf, replacement);
		    i += oper1->data.string->length;
		    j += k;
		} else {
		    if ((j + 1) > BUFFER_LEN)
			abort_interp("Operation would result in overflow.");
		    buf[j++] = xbuf[i++];
		    buf[j] = '\0';
		}
	    }
	}
    }
    CLEAR(oper1);
    CLEAR(oper2);
    CLEAR(oper3);
    PushString(buf);
}

void 
prim_instr(PRIM_PROTOTYPE)
{
    CHECKOP(2);
    oper1 = POP();
    oper2 = POP();
    if (oper1->type != PROG_STRING)
	abort_interp("Invalid argument type (2)");
    if (!(oper1->data.string))
	abort_interp("Empty string argument (2)");
    if (oper2->type != PROG_STRING)
	abort_interp("Non-string argument (1)");
    if (!oper2->data.string) {
	result = 0;
    } else {

	const char *remaining = oper2->data.string->data;
	const char *match = oper1->data.string->data;
	int     step = 1;

	result = 0;
	do {
	    if (!strncmp(remaining, match, oper1->data.string->length)) {
		result = remaining - oper2->data.string->data + 1;
		break;
	    }
	    remaining += step;
	} while (remaining >= oper2->data.string->data && *remaining);
    }
    CLEAR(oper1);
    CLEAR(oper2);
    PushInt(result);
}

void 
prim_rinstr(PRIM_PROTOTYPE)
{
    CHECKOP(2);
    oper1 = POP();
    oper2 = POP();
    if (oper1->type != PROG_STRING)
	abort_interp("Invalid argument type (2)");
    if (!(oper1->data.string))
	abort_interp("Empty string argument (2)");
    if (oper2->type != PROG_STRING)
	abort_interp("Non-string argument (1)");
    if (!oper2->data.string) {
	result = 0;
    } else {

	const char *remaining = oper2->data.string->data;
	const char *match = oper1->data.string->data;
	int     step = -1;

	remaining += oper2->data.string->length - 1;

	result = 0;
	do {
	    if (!strncmp(remaining, match, oper1->data.string->length)) {
		result = remaining - oper2->data.string->data + 1;
		break;
	    }
	    remaining += step;
	} while (remaining >= oper2->data.string->data && *remaining);
    }
    CLEAR(oper1);
    CLEAR(oper2);
    PushInt(result);
}

void 
prim_pronoun_sub(PRIM_PROTOTYPE)
{
    CHECKOP(2);
    oper1 = POP();
    oper2 = POP();		/* oper1 is a string, oper2 a dbref */
    if (!valid_object(oper2))
	abort_interp("Invalid argument (1)");
    if (oper1->type != PROG_STRING)
	abort_interp("Invalid argument (2)");
    if (oper1->data.string) {
	strcpy(buf, pronoun_substitute(fr->descr, oper2->data.objref,
				       oper1->data.string->data));
    } else {
	buf[0] = '\0';
    }
    CLEAR(oper1);
    CLEAR(oper2);
    PushString(buf);
}

void 
prim_toupper(PRIM_PROTOTYPE)
{
    CHECKOP(1);
    oper1 = POP();
    if (oper1->type != PROG_STRING)
	abort_interp("Non-string argument.");
    if (oper1->data.string) {
	strcpy(buf, oper1->data.string->data);
    } else {
	buf[0] = '\0';
    }
    for (ref = 0; buf[ref]; ref++)
	buf[ref] = UPCASE(buf[ref]);
    CLEAR(oper1);
    PushString(buf);
}

void 
prim_tolower(PRIM_PROTOTYPE)
{
    CHECKOP(1);
    oper1 = POP();
    if (oper1->type != PROG_STRING)
	abort_interp("Non-string argument.");
    if (oper1->data.string) {
	strcpy(buf, oper1->data.string->data);
    } else {
	buf[0] = '\0';
    }
    for (ref = 0; buf[ref]; ref++)
	buf[ref] = DOWNCASE(buf[ref]);
    CLEAR(oper1);
    PushString(buf);
}

void 
prim_unparseobj(PRIM_PROTOTYPE)
{
    char tbuf[BUFFER_LEN];
    CHECKOP(1);
    oper1 = POP();
    if (oper1->type != PROG_OBJECT)
	abort_interp("Non-object argument.");
    {
	result = oper1->data.objref;
	switch (result) {
	    case NOTHING:
		sprintf(buf, "*NOTHING*");
		break;
	    case HOME:
		sprintf(buf, "*HOME*");
		break;
	    default:
		if (result < 0 || result > db_top)
		    sprintf(buf, "*INVALID*");
		else
		    sprintf(buf, "%s(#%d%s)", RNAME(result), result,
			    unparse_flags(result, tbuf));
	}
	CLEAR(oper1);
	PushString(buf);
    }
}

void 
prim_smatch(PRIM_PROTOTYPE)
{
    CHECKOP(2);
    oper1 = POP();
    oper2 = POP();
    if (oper1->type != PROG_STRING || oper2->type != PROG_STRING)
	abort_interp("Non-string argument.");
    if (!oper1->data.string || !oper2->data.string)
	abort_interp("Null string argument.");
    {
	char    xbuf[BUFFER_LEN];

	strcpy(buf, oper1->data.string->data);
	strcpy(xbuf, oper2->data.string->data);
	CLEAR(oper1);
	CLEAR(oper2);
	result = equalstr(buf, xbuf);
	PushInt(result);
    }
}

void 
prim_striplead(PRIM_PROTOTYPE)
{				/* string -- string' */
    CHECKOP(1);
    oper1 = POP();
    if (oper1->type != PROG_STRING)
	abort_interp("Not a string argument.");
    strcpy(buf, DoNullInd(oper1->data.string));
    for (pname = buf; *pname && isspace(*pname); pname++);
    CLEAR(oper1);
    PushString(pname);
}

void 
prim_striptail(PRIM_PROTOTYPE)
{				/* string -- string' */
    CHECKOP(1);
    oper1 = POP();
    if (oper1->type != PROG_STRING)
	abort_interp("Not a string argument.");
    strcpy(buf, DoNullInd(oper1->data.string));
    result = strlen(buf);
    while ((result-- > 0) && isspace(buf[result]))
	buf[result] = '\0';
    CLEAR(oper1);
    PushString(buf);
}

void 
prim_stringpfx(PRIM_PROTOTYPE)
{
    CHECKOP(2);
    oper1 = POP();
    oper2 = POP();
    if (oper1->type != PROG_STRING || oper2->type != PROG_STRING)
	abort_interp("Non-string argument.");
    if (oper1->data.string == oper2->data.string)
	result = 0;
    else if (!(oper2->data.string && oper1->data.string))
	result = oper1->data.string ? -1 : 1;
    else {
	result = string_prefix(oper2->data.string->data, oper1->data.string->data);
    }
    CLEAR(oper1);
    CLEAR(oper2);
    PushInt(result);
}


void 
prim_strencrypt(PRIM_PROTOTYPE)
{
    const char *ptr;

    CHECKOP(2);
    oper2 = POP();
    oper1 = POP();
    if (oper1->type != PROG_STRING || oper2->type != PROG_STRING) {
	abort_interp("Non-string argument.");
    }
    if (!oper2->data.string || !*(oper2->data.string->data)) {
	abort_interp("Key cannot be a null string. (2)");
    }
    ptr = strencrypt(DoNullInd(oper1->data.string), oper2->data.string->data);
    CLEAR(oper1);
    CLEAR(oper2);
    PushString(ptr);
}


void 
prim_strdecrypt(PRIM_PROTOTYPE)
{
    const char *ptr;

    CHECKOP(2);
    oper2 = POP();
    oper1 = POP();
    if (oper1->type != PROG_STRING || oper2->type != PROG_STRING) {
	abort_interp("Non-string argument.");
    }
    if (!oper2->data.string || !*(oper2->data.string->data)) {
	abort_interp("Key cannot be a null string. (2)");
    }
    ptr = strdecrypt(DoNullInd(oper1->data.string), oper2->data.string->data);
    CLEAR(oper1);
    CLEAR(oper2);
    PushString(ptr);
}


void
prim_tokensplit(PRIM_PROTOTYPE)
{								/* string delims escapechar -- prestr poststr charstr */
	char *ptr, *delim, *out;
	char esc;
	char outbuf[BUFFER_LEN];

	CHECKOP(3);
	oper3 = POP();
	oper2 = POP();
	oper1 = POP();
	if (oper1->type != PROG_STRING)
		abort_interp("Not a string argument. (1)");
	if (oper2->type != PROG_STRING)
		abort_interp("Not a string argument. (2)");
	if (oper3->type != PROG_STRING)
		abort_interp("Not a string argument. (3)");
	if (!oper2->data.string || oper2->data.string->length < 1)
		abort_interp("Invalid null delimiter string. (2)");
	if (oper3->data.string) {
		esc = oper3->data.string->data[0];
	} else {
		esc = '\0';
	}
	strcpy(buf, DoNullInd(oper1->data.string));
	ptr = buf;
	out = outbuf;
	while (*ptr) {
		if (*ptr == esc) {
			ptr++;
			if (!*ptr)
				break;
		} else {
			delim = oper2->data.string->data;
			while (*delim) {
				if (*delim == *ptr)
					break;
				delim++;
			}
			if (*delim == *ptr)
				break;
		}
		*out++ = *ptr++;
	}
	*out = '\0';
	CLEAR(oper1);
	CLEAR(oper2);
	CLEAR(oper3);
	if (ptr && *ptr) {
		char charbuf[2];

		charbuf[0] = *ptr;
		charbuf[1] = '\0';
		*ptr++ = '\0';
		PushString(outbuf);
		PushString(ptr);
		PushString(charbuf);
	} else {
		PushString(outbuf);
		PushNullStr;
		PushNullStr;
	}
}

void
prim_parse_ansi(PRIM_PROTOTYPE)
{
   char buf3[BUFFER_LEN];
   char buf4[BUFFER_LEN];
   int  ctype;

   CHECKOP(2);
   oper1 = POP();
   oper2 = POP();

   if (oper1->type != PROG_INTEGER)
      abort_interp("Non-integer argument.");
   if (oper2->type != PROG_STRING)
      abort_interp("Not a string argument.");

   if (oper1->data.number < 0 || oper1->data.number > 2)
      abort_interp("Integer out of range of 0-2.");
   if (!oper2->data.string || oper2->data.string->length < 1) {
      CLEAR(oper1);
      CLEAR(oper2);
      PushNullStr;
   } else {

      ctype = oper1->data.number;
      sprintf(buf3, "%s", oper2->data.string->data);
      CLEAR(oper1);
      CLEAR(oper2);

      if (ctype == 0)
        sprintf(buf4, "%s", buf3);
      if (ctype == 1)
         sprintf(buf4, "%s", parse_ansi(buf, buf3));
      if (ctype == 2)
        sprintf(buf4, "%s", parse_mush_ansi(buf, buf3));

      PushString(buf4);
   }
}

void
prim_unparse_ansi(PRIM_PROTOTYPE)
{
   char buf3[BUFFER_LEN];
   char buf4[BUFFER_LEN];
   int  ctype;

   CHECKOP(2);
   oper1 = POP();
   oper2 = POP();

   if (oper1->type != PROG_INTEGER)
      abort_interp("Non-integer argument.");
   if (oper2->type != PROG_STRING)
      abort_interp("Not a string argument.");

   if (oper1->data.number < 0 || oper1->data.number > 2)
      abort_interp("Integer out of range of 0-2.");
   if (!oper2->data.string || oper2->data.string->length < 1) {
      CLEAR(oper1);
      CLEAR(oper2);
      PushNullStr;
   } else {

      ctype = oper1->data.number;
      sprintf(buf3, "%s", oper2->data.string->data);
      CLEAR(oper1);
      CLEAR(oper2);

      if (ctype == 0) {
         strip_ansi(buf, buf3);
         sprintf(buf4, "%s", buf);
      }
      if (ctype == 1) {
         unparse_ansi(buf, buf3);
         sprintf(buf4, "%s", buf);
      }
      if (ctype == 2)
         sprintf(buf4, "%s", unparse_mush_ansi(buf, buf3));

      PushString(buf4);
   }
}

void
prim_escape_ansi(PRIM_PROTOTYPE)
{
   char buf3[BUFFER_LEN];
   char buf4[BUFFER_LEN];
   int  ctype;

   CHECKOP(2);
   oper1 = POP();
   oper2 = POP();

   if (oper1->type != PROG_INTEGER)
      abort_interp("Non-integer argument.");
   if (oper2->type != PROG_STRING)
      abort_interp("Not a string argument.");

   if (oper1->data.number < 0 || oper1->data.number > 2)
      abort_interp("Integer out of range of 0-2.");
   if (!oper2->data.string || oper2->data.string->length < 1) {
      CLEAR(oper1);
      CLEAR(oper2);
      PushNullStr;
   } else {

      ctype = oper1->data.number;
      sprintf(buf3, "%s", oper2->data.string->data);
      CLEAR(oper1);
      CLEAR(oper2);

      if (ctype == 0)
         sprintf(buf4, "%s", escape_ansi(buf, buf3));
      if (ctype == 1) {
         sprintf(buf4, "%s", tct(buf3, buf));
      }
      if (ctype == 2)
         sprintf(buf4, "%s", mush_tct(buf3, buf));

      PushString(buf4);
   }
}



void
prim_ansi_strlen(PRIM_PROTOTYPE)
{
	char *ptr;
	int i;

	CHECKOP(1);
	oper1 = POP();
	if (oper1->type != PROG_STRING)
		abort_interp("Not a string argument.");

	if (!oper1->data.string) {
		CLEAR(oper1);
		i = 0;
		PushInt(i);
		/* Weird PushInt() #define requires that. */
		return;
	}

	i = 0;

	ptr = oper1->data.string->data;

	while (*ptr) {
		if (*ptr++ == ESCAPE_CHAR) {
			if (*ptr == '\0') {;
			} else if (*ptr != '[') {
				ptr++;
			} else {
				ptr++;
				while (isdigit(*ptr) || *ptr == ';')
					ptr++;
				if (*ptr == 'm')
					ptr++;
			}
		} else {
			i++;
		}
	}
	CLEAR(oper1);
	PushInt(i);
}

void
prim_ansi_strcut(PRIM_PROTOTYPE)
{
	char *ptr;
	char *op;
	char outbuf1[BUFFER_LEN];
	char outbuf2[BUFFER_LEN];
	int loc;

	CHECKOP(2);
	oper2 = POP();
	oper1 = POP();
	if (oper1->type != PROG_STRING)
		abort_interp("Not a string argument. (1)");
	if (oper2->type != PROG_INTEGER)
		abort_interp("Not an integer argument. (2)");
	if (!oper1->data.string) {
		CLEAR(oper1);
		CLEAR(oper2);
		PushNullStr;
		PushNullStr;
		return;
	}

	loc = 0;

	if (oper2->data.number >= oper1->data.string->length) {
		strcpy(buf, oper1->data.string->data);
		CLEAR(oper1);
		CLEAR(oper2);
		PushString(buf);
		PushNullStr;
		return;
	} else if (oper2->data.number <= 0) {
		strcpy(buf, oper1->data.string->data);
		CLEAR(oper1);
		CLEAR(oper2);
		PushNullStr;
		PushString(buf);
		return;
	}

	ptr = oper1->data.string->data;

	*outbuf2 = '\0';
	op = outbuf1;
	while (*ptr) {
		if ((*op++ = *ptr++) == ESCAPE_CHAR) {
			if (*ptr == '\0') {
				*op++ = *ptr++;
			} else if (*ptr != '[') {
				*op++ = *ptr++;
			} else {
				*op++ = *ptr++;
				while (isdigit(*ptr) || *ptr == ';')
					*op++ = *ptr++;
				if (*ptr == 'm')
					*op++ = *ptr++;
			}
		} else {
			loc++;
			if (loc == oper2->data.number) {
				break;
			}
		}
	}
	*op = '\0';
	memcpy((void *) outbuf2, (const void *) ptr,
		   oper1->data.string->length - (ptr - oper1->data.string->data) + 1);

	CLEAR(oper1);
	CLEAR(oper2);
	PushString(outbuf1);
	if (!*outbuf2) {
		PushNullStr;
	} else {
		PushString(outbuf2);
	}
}

void
prim_ansi_strip(PRIM_PROTOTYPE)
{
	CHECKOP(1);
	oper1 = POP();
	if (oper1->type != PROG_STRING)
		abort_interp("Non-string argument.");

	if (!oper1->data.string) {
		CLEAR(oper1);
		PushNullStr;
		return;
	}

	strip_ansi(buf, oper1->data.string->data);
	CLEAR(oper1);
	PushString(buf);
}



void
prim_textattr(PRIM_PROTOTYPE)
{
	int totallen;
	int done = 0;
	const char *ptr;
	char *ptr2;
	char buf[BUFFER_LEN];
	char attr[BUFFER_LEN];

	CHECKOP(2);
	oper2 = POP();
	oper1 = POP();
	if (oper1->type != PROG_STRING) {
		abort_interp("Non-string argument. (1)");
	}
	if (oper2->type != PROG_STRING) {
		abort_interp("Non-string argument. (2)");
	}

	if (!oper1->data.string || !oper2->data.string) {
		strcpy(buf, DoNullInd(oper1->data.string));
	} else {
		*buf = '\0';
		ptr = oper2->data.string->data;
		ptr2 = attr;
		while (!done) {
			switch (*ptr) {
			case ' ':{
					ptr++;
					break;
				}

			case '\0':
			case ',':{
					*ptr2++ = '\0';
					if (!string_compare(attr, "reset")) {
						strcat(buf, ANSI_RESET);
					} else if (!string_compare(attr, "bold")) {
						strcat(buf, ANSI_BOLD);
					} else if (!string_compare(attr, "dim")) {
						strcat(buf, ANSI_DIM);
					} else if (!string_compare(attr, "uline") ||
							   !string_compare(attr, "underline")) {
						strcat(buf, ANSI_UNDERLINE);
					} else if (!string_compare(attr, "flash")) {
						strcat(buf, ANSI_FLASH);
					} else if (!string_compare(attr, "reverse")) {
						strcat(buf, ANSI_REVERSE);

					} else if (!string_compare(attr, "black")) {
						strcat(buf, ANSI_FG_BLACK);
					} else if (!string_compare(attr, "red")) {
						strcat(buf, ANSI_FG_RED);
					} else if (!string_compare(attr, "yellow")) {
						strcat(buf, ANSI_FG_YELLOW);
					} else if (!string_compare(attr, "green")) {
						strcat(buf, ANSI_FG_GREEN);
					} else if (!string_compare(attr, "cyan")) {
						strcat(buf, ANSI_FG_CYAN);
					} else if (!string_compare(attr, "blue")) {
						strcat(buf, ANSI_FG_BLUE);
					} else if (!string_compare(attr, "magenta")) {
						strcat(buf, ANSI_FG_MAGENTA);
					} else if (!string_compare(attr, "white")) {
						strcat(buf, ANSI_FG_WHITE);

					} else if (!string_compare(attr, "bg_black")) {
						strcat(buf, ANSI_BG_BLACK);
					} else if (!string_compare(attr, "bg_red")) {
						strcat(buf, ANSI_BG_RED);
					} else if (!string_compare(attr, "bg_yellow")) {
						strcat(buf, ANSI_BG_YELLOW);
					} else if (!string_compare(attr, "bg_green")) {
						strcat(buf, ANSI_BG_GREEN);
					} else if (!string_compare(attr, "bg_cyan")) {
						strcat(buf, ANSI_BG_CYAN);
					} else if (!string_compare(attr, "bg_blue")) {
						strcat(buf, ANSI_BG_BLUE);
					} else if (!string_compare(attr, "bg_magenta")) {
						strcat(buf, ANSI_BG_MAGENTA);
					} else if (!string_compare(attr, "bg_white")) {
						strcat(buf, ANSI_BG_WHITE);
					} else {
						abort_interp("Unrecognized attribute tag.  Try one of reset, bold, dim, underline, reverse, black, red, yellow, green, cyan, blue, magenta, white, bg_black, bg_red, bg_yellow, bg_green, bg_cyan, bg_blue, bg_magenta, or bg_white.");
					}

					ptr2 = attr;
					if (!*ptr) {
						done++;
					} else {
						ptr++;
					}
					break;
				}

			default:{
					*ptr2++ = *ptr++;
				}
			}
		}
		totallen = strlen(buf);
		totallen += oper1->data.string->length;
		totallen += strlen(ANSI_RESET);
		if (totallen >= BUFFER_LEN) {
			abort_interp("Operation would result in too long of a string.");
		}
		strcat(buf, oper1->data.string->data);
	}
	strcat(buf, ANSI_RESET);

	CLEAR(oper1);
	CLEAR(oper2);
	PushString(buf);
}



