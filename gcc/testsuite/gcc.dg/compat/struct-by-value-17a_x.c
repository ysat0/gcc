#include "compat-common.h"

#include "fp-struct-defs.h"
#include "fp-struct-check.h"
#include "fp-struct-test-by-value-x.h"

DEFS(cd, _Complex double)
CHECKS(cd, _Complex double)


TEST(Scd13, _Complex double)
TEST(Scd14, _Complex double)
TEST(Scd15, _Complex double)
TEST(Scd16, _Complex double)

#undef T

void
struct_by_value_17a_x ()
{
DEBUG_INIT

#define T(TYPE, MTYPE) testit##TYPE ();


T(Scd13, _Complex double)
T(Scd14, _Complex double)
T(Scd15, _Complex double)
T(Scd16, _Complex double)

DEBUG_FINI

if (fails != 0)
  abort ();

#undef T
}
#include "compat-common.h"

#include "fp-struct-defs.h"
#include "fp-struct-check.h"
#include "fp-struct-test-by-value-x.h"

DEFS(cd, _Complex double)
CHECKS(cd, _Complex double)


TEST(Scd13, _Complex double)
TEST(Scd14, _Complex double)
TEST(Scd15, _Complex double)
TEST(Scd16, _Complex double)

#undef T

void
struct_by_value_17a_x ()
{
DEBUG_INIT

#define T(TYPE, MTYPE) testit##TYPE ();


T(Scd13, _Complex double)
T(Scd14, _Complex double)
T(Scd15, _Complex double)
T(Scd16, _Complex double)

DEBUG_FINI

if (fails != 0)
  abort ();

#undef T
}
