/* LongViewBufferImpl.java -- 
   Copyright (C) 2003 Free Software Foundation, Inc.

This file is part of GNU Classpath.

GNU Classpath is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU Classpath is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU Classpath; see the file COPYING.  If not, write to the
Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
02111-1307 USA.

Linking this library statically or dynamically with other modules is
making a combined work based on this library.  Thus, the terms and
conditions of the GNU General Public License cover the whole
combination.

As a special exception, the copyright holders of this library give you
permission to link this library with independent modules to produce an
executable, regardless of the license terms of these independent
modules, and to copy and distribute the resulting executable under
terms of your choice, provided that you also meet, for each linked
independent module, the terms and conditions of the license of that
module.  An independent module is a module which is not derived from
or based on this library.  If you modify this library, you may extend
this exception to your version of the library, but you are not
obligated to do so.  If you do not wish to do so, delete this
exception statement from your version. */


package java.nio;

class LongViewBufferImpl extends LongBuffer
{
  private boolean readOnly;
  private int offset;
  private ByteBuffer bb;
  private ByteOrder endian;
  
  public LongViewBufferImpl (ByteBuffer bb, boolean readOnly)
  {
    super (bb.remaining () >> 3, bb.remaining () >> 3, bb.position (), 0);
    this.bb = bb;
    this.readOnly = readOnly;
    // FIXME: What if this is called from LongByteBufferImpl and ByteBuffer has changed its endianess ?
    this.endian = bb.order ();
  }

  public LongViewBufferImpl (ByteBuffer bb, int offset, int capacity,
                               int limit, int position, int mark,
                               boolean readOnly)
  {
    super (limit >> 3, limit >> 3, position >> 3, mark >> 3);
    this.bb = bb;
    this.offset = offset;
    this.readOnly = readOnly;
    // FIXME: What if this is called from LongViewBufferImpl and ByteBuffer has changed its endianess ?
    this.endian = bb.order ();
  }

  public long get ()
  {
    long result = bb.getLong ((position () << 3) + offset);
    position (position () + 1);
    return result;
  }

  public long get (int index)
  {
    return bb.getLong ((index << 3) + offset);
  }

  public LongBuffer put (long value)
  {
    bb.putLong ((position () << 3) + offset, value);
    position (position () + 1);
    return this;
  }
  
  public LongBuffer put (int index, long value)
  {
    bb.putLong ((index << 3) + offset, value);
    return this;
  }

  public LongBuffer compact ()
  {
    if (position () > 0)
      {
        // Copy all data from position() to limit() to the beginning of the
        // buffer, set position to end of data and limit to capacity
        // XXX: This can surely be optimized, for direct and non-direct buffers
        
        int count = limit () - position ();
              
        for (int i = 0; i < count; i++)
          {
            bb.putLong ((i >> 3) + offset,
                          bb.getLong (((i + position ()) >> 3) + offset));
          }

        position (count);
        limit (capacity ());
      }

    return this;
  }
  
  public LongBuffer duplicate ()
  {
    // Create a copy of this object that shares its content
    // FIXME: mark is not correct
    return new LongViewBufferImpl (bb, offset, capacity (), limit (),
                                     position (), -1, isReadOnly ());
  }
  
  public LongBuffer slice ()
  {
    // Create a sliced copy of this object that shares its content.
    return new LongViewBufferImpl (bb, (position () >> 3) + offset,
                                      remaining (), remaining (), 0, -1,
                                     isReadOnly ());
  }
  
  public LongBuffer asReadOnlyBuffer ()
  {
    // Create a copy of this object that shares its content and is read-only
    return new LongViewBufferImpl (bb, (position () >> 3) + offset,
                                     remaining (), remaining (), 0, -1, true);
  }
  
  public boolean isReadOnly ()
  {
    return readOnly;
  }
  
  public boolean isDirect ()
  {
    return bb.isDirect ();
  }
  
  public ByteOrder order ()
  {
    return ByteOrder.LITTLE_ENDIAN;
  }
}
