��print *, "Hello world!"
��end ! { dg-error "Invalid character" }
! { dg-do compile }
! { dg-excess-errors "Unexpected end of file" }
