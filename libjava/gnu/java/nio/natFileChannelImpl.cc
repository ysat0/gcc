// natFileChannelImpl.cc

/* Copyright (C) 2003  Free Software Foundation

   This file is part of libgcj.

This software is copyrighted work licensed under the terms of the
Libgcj License.  Please consult the file "LIBGCJ_LICENSE" for
details.  */

#include <config.h>

#include <jvm.h>

#include <errno.h>
#include <string.h>
#include <sys/types.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#include <gnu/gcj/RawData.h>
#include <gnu/java/nio/FileChannelImpl.h>
#include <java/io/FileDescriptor.h>
#include <java/io/IOException.h>
#include <java/nio/ByteBuffer.h>
#include <java/nio/channels/FileChannel.h>

jlong
gnu::java::nio::FileChannelImpl::size ()
{
  return fd->length ();
}

jlong
gnu::java::nio::FileChannelImpl::implPosition ()
{
  return fd->getFilePointer ();
}

java::nio::channels::FileChannel*
gnu::java::nio::FileChannelImpl::implPosition (jlong newPosition)
{
  fd->seek (newPosition, ::java::io::FileDescriptor::SET, true);
  return this;
}

jint
gnu::java::nio::FileChannelImpl::implRead (JArray<jbyte>* buffer,
                                           jint offset, jint len)
{
  return fd->read (buffer, offset, len);
}

jint
gnu::java::nio::FileChannelImpl::implWrite (JArray<jbyte>* buffer,
                                            jint offset, jint len)
{
  fd->write (buffer, offset, len);
  return len;
}

java::nio::channels::FileChannel*
gnu::java::nio::FileChannelImpl::implTruncate (jlong size)
{
  fd->setLength (size);
  return this;
}

gnu::gcj::RawData*
gnu::java::nio::FileChannelImpl::nio_mmap_file (jlong pos, jlong size, jint /*mode*/)
{
  throw new ::java::io::IOException (JvNewStringUTF ("mmap not implemented"));
}

void
gnu::java::nio::FileChannelImpl::nio_unmmap_file (gnu::gcj::RawData* map_address, jint size)
{
  throw new ::java::io::IOException (JvNewStringUTF ("munmap not implemented"));
}

void
gnu::java::nio::FileChannelImpl::nio_msync (gnu::gcj::RawData* map_address, jint length)
{
  throw new ::java::io::IOException (JvNewStringUTF ("msync not implemented"));
}
