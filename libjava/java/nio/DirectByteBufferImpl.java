/* DirectByteBufferImpl.java -- 
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

import gnu.gcj.RawData;

class DirectByteBufferImpl extends ByteBuffer
{
  RawData address;
  private int offset;
  private boolean readOnly;

  public DirectByteBufferImpl (RawData address, long len)
  {
    this (address, 0, (int) len, (int) len, 0, -1, false);
  }
  
  public DirectByteBufferImpl (RawData address, int offset, int capacity,
                               int limit, int position, int mark,
                               boolean readOnly)
  {
    super (capacity, limit, position, mark);
    this.address = address;
    this.offset = offset;
    this.readOnly = readOnly;
  }

  private static native RawData allocateImpl (int capacity);
  private static native void freeImpl (RawData address);
  
  protected void finalize () throws Throwable
  {
    freeImpl (address);
  }
  
  public static ByteBuffer allocateDirect (int capacity)
  {
    RawData address = allocateImpl (capacity);

    if (address == null)
      throw new InternalError ("Not enough memory to create direct buffer");
    
    return new DirectByteBufferImpl (address, 0, capacity, capacity, 0, -1, false);
  }
  
  private native byte getImpl (int index);
  private native void putImpl (int index, byte value);

  public byte get ()
  {
    byte result = getImpl (position () + offset);
    position (position () + 1);
    return result;
  }

  public byte get (int index)
  {
    return getImpl (index);
  }

  public ByteBuffer put (byte value)
  {
    putImpl (position (), value);
    position (position () + 1);
    return this;
  }
  
  public ByteBuffer put (int index, byte value)
  {
    putImpl (index, value);
    return this;
  }
  
  public ByteBuffer compact ()
  {
    // FIXME this can sure be optimized using memcpy()  
    int copied = 0;
    
    while (remaining () > 0)
      {
	put (copied, get ());
	copied++;
      }

    position (copied);
    return this;
  }

  public ByteBuffer duplicate ()
  {
    return new DirectByteBufferImpl (
      address, offset, capacity (), limit (), position (), -1, isReadOnly ());
  }

  public ByteBuffer slice ()
  {
    return new DirectByteBufferImpl (address, position () + offset, remaining (), remaining (), 0, -1, isReadOnly ());
  }

  public ByteBuffer asReadOnlyBuffer ()
  {
    return new DirectByteBufferImpl (
      address, offset, capacity (), limit (), position (), -1, true);
  }

  public boolean isReadOnly ()
  {
    return readOnly;
  }

  public boolean isDirect ()
  {
    return true;
  }

  public CharBuffer asCharBuffer ()
  {
    return new CharViewBufferImpl (this, position () + offset, remaining (), remaining (), 0, -1, isReadOnly ());
  }
  
  public DoubleBuffer asDoubleBuffer ()
  {
    return new DoubleViewBufferImpl (this, position () + offset, remaining (), remaining (), 0, -1, isReadOnly ());
  }
  
  public FloatBuffer asFloatBuffer ()
  {
    return new FloatViewBufferImpl (this, position () + offset, remaining (), remaining (), 0, -1, isReadOnly ());
  }
  
  public IntBuffer asIntBuffer ()
  {
    return new IntViewBufferImpl (this, position () + offset, remaining (), remaining (), 0, -1, isReadOnly ());
  }
  
  public LongBuffer asLongBuffer ()
  {
    return new LongViewBufferImpl (this, position () + offset, remaining (), remaining (), 0, -1, isReadOnly ());
  }
  
  public ShortBuffer asShortBuffer ()
  {
    return new ShortViewBufferImpl (this, position () + offset, remaining (), remaining (), 0, -1, isReadOnly ());
  }
  
  final public char getChar ()
  {
    // FIXME: this handles little endian only
    return (char) (((get () & 0xff) << 8)
                   + (get () & 0xff));
  }
  
  final public ByteBuffer putChar (char value)
  {
    // FIXME: this handles little endian only
    put ((byte) ((((int) value) & 0xff00) >> 8));
    put ((byte) (((int) value) & 0x00ff));
    return this;
  }
  
  final public char getChar (int index)
  {
    // FIXME: this handles little endian only
    return (char) (((get (index) & 0xff) << 8)
                   + (get (index + 1) & 0xff));
  }
  
  final public ByteBuffer putChar (int index, char value)
  {
    // FIXME: this handles little endian only
    put (index, (byte) ((((int) value) & 0xff00) >> 8));
    put (index + 1, (byte) (((int) value) & 0x00ff));
    return this;
  }

  final public short getShort ()
  {
    // FIXME: this handles little endian only
    return (short) (((get () & 0xff) << 8)
                    + (get () & 0xff));
  }
  
  final public ByteBuffer putShort (short value)
  {
    // FIXME: this handles little endian only
    put ((byte) ((((int) value) & 0xff00) >> 8));
    put ((byte) (((int) value) & 0x00ff));
    return this;
  }
  
  final public short getShort (int index)
  {
    // FIXME: this handles little endian only
    return (short) (((get (index) & 0xff) << 8)
                    + (get (index + 1) & 0xff));
  }
  
  final public ByteBuffer putShort (int index, short value)
  {
    // FIXME: this handles little endian only
    put (index, (byte) ((((int) value) & 0xff00) >> 8));
    put (index + 1, (byte) (((int) value) & 0x00ff));
    return this;
  }

  final public int getInt ()
  {
    // FIXME: this handles little endian only
    return (int) (((get () & 0xff) << 24)
                  + ((get () & 0xff) << 16)
                  + ((get () & 0xff) << 8)
                  + (get () & 0xff));
  }
  
  final public ByteBuffer putInt (int value)
  {
    // FIXME: this handles little endian only
    put ((byte) ((((int) value) & 0xff000000) >> 24));
    put ((byte) ((((int) value) & 0x00ff0000) >> 16));
    put ((byte) ((((int) value) & 0x0000ff00) >> 8));
    put ((byte) (((int) value) & 0x000000ff));
    return this;
  }
  
  final public int getInt (int index)
  {
    // FIXME: this handles little endian only
    return (int) (((get (index) & 0xff) << 24)
                  + ((get (index + 1) & 0xff) << 16)
                  + ((get (index + 2) & 0xff) << 8)
                  + (get (index + 3) & 0xff));
  }
  
  final public ByteBuffer putInt (int index, int value)
  {
    // FIXME: this handles little endian only
    put (index, (byte) ((((int) value) & 0xff000000) >> 24));
    put (index + 1, (byte) ((((int) value) & 0x00ff0000) >> 16));
    put (index + 2, (byte) ((((int) value) & 0x0000ff00) >> 8));
    put (index + 3, (byte) (((int) value) & 0x000000ff));
    return this;
  }

  final public long getLong ()
  {
    // FIXME: this handles little endian only
    return (long) (((get () & 0xff) << 56)
                   + ((get () & 0xff) << 48)
                   + ((get () & 0xff) << 40)
                   + ((get () & 0xff) << 32)
                   + ((get () & 0xff) << 24)
                   + ((get () & 0xff) << 16)
                   + ((get () & 0xff) << 8)
                   + (get () & 0xff));
  }
  
  final public ByteBuffer putLong (long value)
  {
    return ByteBufferHelper.putLong (this, value);
  }
  
  final public long getLong (int index)
  {
    return ByteBufferHelper.getLong (this, index);
  }
  
  final public ByteBuffer putLong (int index, long value)
  {
    return ByteBufferHelper.putLong (this, index, value);
  }

  final public float getFloat ()
  {
    return ByteBufferHelper.getFloat (this);
  }
  
  final public ByteBuffer putFloat (float value)
  {
    return ByteBufferHelper.putFloat (this, value);
  }
  
  public final float getFloat (int index)
  {
    return ByteBufferHelper.getFloat (this, index);
  }

  final public ByteBuffer putFloat (int index, float value)
  {
    return ByteBufferHelper.putFloat (this, index, value);
  }

  final public double getDouble ()
  {
    return ByteBufferHelper.getDouble (this);
  }

  final public ByteBuffer putDouble (double value)
  {
    return ByteBufferHelper.putDouble (this, value);
  }
  
  final public double getDouble (int index)
  {
    return ByteBufferHelper.getDouble (this, index);
  }
  
  final public ByteBuffer putDouble (int index, double value)
  {
    return ByteBufferHelper.putDouble (this, index, value);
  }
}
