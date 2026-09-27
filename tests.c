#define UNIT_TEST
#include "main.c"
#include <stdlib.h>
#define CHECK(condition) do { if (!(condition)) { fprintf(stderr, "CHECK failed at line %d: %s\n", __LINE__, #condition); exit(1); } } while (0)
#include <string.h>

int main(void) {
    Fields f;
    FILE *schema = tmpfile(), *data = tmpfile(), *out = tmpfile();
    char report[4096] = {0};
    CHECK(parse_csv_line("a,\"b,c\",\"x\"\"y\"", &f));
    CHECK(f.count == 3 && strcmp(f.value[1], "b,c") == 0 && strcmp(f.value[2], "x\"y") == 0);
    CHECK(!parse_csv_line("a,\"unterminated", &f));
    CHECK(!parse_csv_line("a,\"x\"tail", &f));
    CHECK(valid_type("-12", "int", 1));
    CHECK(!valid_type("12x", "int", 1));
    CHECK(!valid_type("NaN", "decimal", 1));
    CHECK(!valid_type("0x1p4", "decimal", 1));
    CHECK(!valid_type("", "text", 1));
    fputs("id,int,required\nprice,decimal,required\nnote,text,optional\n", schema);
    fputs("id,price,note\n1,2.50,ok\n2,broken,wrong\n3,4.25,\n", data);
    rewind(schema); rewind(data);
    CHECK(run_validation(schema, data, out) == 0);
    rewind(out); fread(report, 1, sizeof(report)-1, out);
    CHECK(strstr(report, "rows=3 valid=2 invalid=1") != NULL);
    CHECK(strstr(report, "line=3") != NULL);
    fclose(schema); fclose(data); fclose(out);
    schema=tmpfile(); data=tmpfile(); out=tmpfile();
    CHECK(schema && data && out);
    fputs("id,int,required\n",schema);
    fputs("other\n1\n",data);
    rewind(schema); rewind(data);
    CHECK(run_validation(schema,data,out)!=0);
    fclose(schema); fclose(data); fclose(out);
    schema=tmpfile(); data=tmpfile(); out=tmpfile();
    CHECK(schema && data && out);
    fputs("name,text,required\n",schema);
    const char embedded[]="name\nok\0junk\nnext\n";
    CHECK(fwrite(embedded,1,sizeof(embedded)-1,data)==sizeof(embedded)-1);
    rewind(schema); rewind(data);
    CHECK(run_validation(schema,data,out)==0);
    rewind(out); memset(report,0,sizeof(report));
    CHECK(fread(report,1,sizeof(report)-1,out)>0);
    CHECK(strstr(report,"rows=2 valid=1 invalid=1")!=NULL);
    CHECK(strstr(report,"line=2 status=invalid")!=NULL);
    fclose(schema); fclose(data); fclose(out);
    schema=tmpfile(); data=tmpfile(); out=tmpfile();
    CHECK(schema && data && out);
    fputs("name,text,required\n",schema);
    const char final_nul[]="name\nbad\0tail";
    CHECK(fwrite(final_nul,1,sizeof(final_nul)-1,data)==sizeof(final_nul)-1);
    rewind(schema); rewind(data);
    CHECK(run_validation(schema,data,out)==0);
    rewind(out); memset(report,0,sizeof(report));
    CHECK(fread(report,1,sizeof(report)-1,out)>0);
    CHECK(strstr(report,"rows=1 valid=0 invalid=1")!=NULL);
    fclose(schema); fclose(data); fclose(out);
    puts("csv tests passed");
    return 0;
}
