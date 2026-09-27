# CSV schema validator (C11)

Check small CSV imports against a typed schema before loading them elsewhere. The synthetic order data includes a quoted comma and one bad decimal. `examples/report.txt` records the resulting row-quality counts and error line.

```sh
cc -std=c11 -O2 -Wall -Wextra -pedantic main.c -o csv-validator
./csv-validator fixtures/schema.csv fixtures/orders.csv
cc -std=c11 -O2 -Wall -Wextra -pedantic -Wno-unused-function tests.c -o tests
./tests
```

The schema has no header and uses `name,type,required|optional`, where type is `int`, `decimal`, or `text`. The data header must match schema names and order. Data supports quoted fields, embedded commas, and doubled quotes; embedded newlines are unsupported. At most 32 columns, 255 bytes per field, and 4,094 bytes per physical line are supported. Integers are signed 64-bit decimal; decimals use ordinary decimal or exponent notation and must be finite. Hex floats and embedded NUL bytes are rejected. An optional empty value is valid. An invalid data row contributes once to the invalid count and prints its first error; an invalid schema or mismatched header exits nonzero.

Sample summary: `rows=3 valid=2 invalid=1`.
