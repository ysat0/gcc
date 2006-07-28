// natVMAccessController.cc -- Native part of the VMAccessController class.

/* Copyright (C) 2006 Free Software Foundation, Inc.

   This file is part of libgcj.

This software is copyrighted work licensed under the terms of the
Libgcj License.  Please consult the file "LIBGCJ_LICENSE" for
details.  */

#include <config.h>

#include <gcj/cni.h>
#include <jvm.h>
#include <java-stack.h>

#include <java/security/VMAccessController.h>

JArray<jobjectArray> *
java::security::VMAccessController::getStack ()
{
  _Jv_StackTrace *trace = _Jv_StackTrace::GetStackTrace ();
  return _Jv_StackTrace::GetClassMethodStack (trace);
}

jboolean
java::security::VMAccessController::runtimeInitialized ()
{
  return gcj::runtimeInitialized;
}
