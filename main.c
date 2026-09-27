#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_COLUMNS 32
#define MAX_FIELD 256
#define MAX_LINE 4096
typedef struct { size_t count; char value[MAX_COLUMNS][MAX_FIELD]; } Fields;
typedef struct { char name[MAX_FIELD], type[16]; int required; } Column;

static int parse_csv_line(const char *line, Fields *fields) {
    const char *p=line;
    memset(fields,0,sizeof(*fields));
    for (;;) {
        if (fields->count == MAX_COLUMNS) return 0;
        char *dest=fields->value[fields->count++];
        size_t n=0;
        if (*p=='"') {
            ++p;
            for (;;) {
                if (!*p) return 0;
                if (*p=='"') {
                    ++p;
                    if (*p=='"') { if (n==MAX_FIELD-1) return 0; dest[n++]='"'; ++p; }
                    else break;
                } else {
                    if (n==MAX_FIELD-1) return 0;
                    dest[n++]=*p++;
                }
            }
            if (*p && *p!=',') return 0;
        } else {
            while (*p && *p!=',') {
                if (*p=='"' || n==MAX_FIELD-1) return 0;
                dest[n++]=*p++;
            }
        }
        dest[n]='\0';
        if (!*p) return 1;
        ++p;
    }
}
static int valid_type(const char *value, const char *type, int required) {
    char *end;
    if (!*value) return !required;
    if (strcmp(type,"text")==0) return 1;
    if (strcmp(type,"int")==0) {
        const char *p=value;
        if (*p=='-' || *p=='+') ++p;
        if (!isdigit((unsigned char)*p)) return 0;
        for(;*p;++p) if(!isdigit((unsigned char)*p)) return 0;
        errno=0; (void)strtoll(value,&end,10);
        return !errno && *end=='\0';
    }
    if (strcmp(type,"decimal")==0) {
        const char *p=value;
        if (*p=='-' || *p=='+') ++p;
        int digits=0;
        while(isdigit((unsigned char)*p)) { ++p; ++digits; }
        if(*p=='.') { ++p; while(isdigit((unsigned char)*p)) { ++p; ++digits; } }
        if(!digits) return 0;
        if(*p=='e' || *p=='E') {
            ++p;
            if(*p=='+' || *p=='-') ++p;
            if(!isdigit((unsigned char)*p)) return 0;
            while(isdigit((unsigned char)*p)) ++p;
        }
        if(*p) return 0;
        errno=0; double d=strtod(value,&end);
        return !errno && *end=='\0' && end!=value && isfinite(d);
    }
    return 0;
}
static int next_line(FILE *file, char line[MAX_LINE], int *too_long) {
    size_t n=0; int c,got=0;
    *too_long=0;
    while((c=fgetc(file))!=EOF) {
        got=1;
        if(c=='\n') break;
        if(c=='\0') { *too_long=1; continue; }
        if(n<MAX_LINE-2) line[n++]=(char)c;
        else *too_long=1;
    }
    if(!got) return 0;
    if(n && line[n-1]=='\r') --n;
    line[n]='\0';
    return 1;
}
static int run_validation(FILE *schema, FILE *data, FILE *out) {
    Column columns[MAX_COLUMNS]; Fields fields;
    char line[MAX_LINE]; size_t ncols=0, rows=0, valid=0, invalid=0, line_no=0;
    int too_long=0;
    while(next_line(schema,line,&too_long)) {
        if (too_long || !parse_csv_line(line,&fields) || fields.count!=3 || ncols==MAX_COLUMNS || !fields.value[0][0]) {
            fputs("Invalid schema row\n",stderr); return 1;
        }
        for (size_t i=0;i<ncols;++i)
            if(strcmp(columns[i].name,fields.value[0])==0) { fputs("Duplicate schema column\n",stderr); return 1; }
        if (strcmp(fields.value[1],"int") && strcmp(fields.value[1],"decimal") && strcmp(fields.value[1],"text")) {
            fputs("Unknown schema type\n",stderr); return 1;
        }
        if (strcmp(fields.value[2],"required") && strcmp(fields.value[2],"optional")) {
            fputs("Expected required or optional in schema\n",stderr); return 1;
        }
        strcpy(columns[ncols].name,fields.value[0]);
        strcpy(columns[ncols].type,fields.value[1]);
        columns[ncols].required=strcmp(fields.value[2],"required")==0;
        ncols++;
    }
    if (ferror(schema) || !ncols) { fputs("Empty or unreadable schema\n",stderr); return 1; }
    if (!next_line(data,line,&too_long) || too_long || !parse_csv_line(line,&fields) || fields.count!=ncols) {
        fputs("Invalid CSV header\n",stderr); return 1;
    }
    for(size_t i=0;i<ncols;++i)
        if(strcmp(fields.value[i],columns[i].name)!=0) { fputs("CSV header does not match schema\n",stderr); return 1; }
    line_no=1;
    while(next_line(data,line,&too_long)) {
        const char *reason=NULL, *column="-";
        ++line_no; ++rows;
        if (too_long || !parse_csv_line(line,&fields)) reason="malformed CSV";
        else if (fields.count!=ncols) reason="wrong column count";
        else for(size_t i=0;i<ncols;++i) {
            if (!valid_type(fields.value[i],columns[i].type,columns[i].required)) {
                reason="invalid or missing value"; column=columns[i].name; break;
            }
        }
        if (reason) { invalid++; fprintf(out,"line=%zu status=invalid column=%s reason=%s\n",line_no,column,reason); }
        else valid++;
    }
    if (ferror(data) || ferror(out)) { fputs("I/O error\n",stderr); return 1; }
    fprintf(out,"rows=%zu valid=%zu invalid=%zu\n",rows,valid,invalid);
    return ferror(out) ? 1 : 0;
}
#ifndef UNIT_TEST
int main(int argc, char **argv) {
    FILE *schema,*data; int result;
    if(argc!=3) { fputs("Usage: csv-validator schema.csv data.csv\n",stderr); return 2; }
    schema=fopen(argv[1],"r");
    if(!schema) { perror(argv[1]); return 2; }
    data=fopen(argv[2],"r");
    if(!data) { perror(argv[2]); fclose(schema); return 2; }
    result=run_validation(schema,data,stdout);
    fclose(schema); fclose(data);
    return result;
}
#endif
