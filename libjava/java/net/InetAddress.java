/* InetAddress.java -- Class to model an Internet address
   Copyright (C) 1998, 1999, 2002 Free Software Foundation, Inc.

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


package java.net;

import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.io.ObjectStreamException;
import java.io.Serializable;

/**
 * This class models an Internet address.  It does not have a public
 * constructor.  Instead, new instances of this objects are created
 * using the static methods getLocalHost(), getByName(), and
 * getAllByName().
 * <p>
 * This class fulfills the function of the C style functions gethostname(),
 * gethostbyname(), and gethostbyaddr().  It resolves Internet DNS names
 * into their corresponding numeric addresses and vice versa.
 *
 * @author Aaron M. Renn <arenn@urbanophile.com>
 * @author Per Bothner
 *
 * @specnote This class is not final since JK 1.4
 */
public class InetAddress implements Serializable
{
  private static final long serialVersionUID = 3286316764910316507L;
  
  static final byte[] zeros = { 0, 0, 0, 0 };
  
  /**
   * Dummy InetAddress, used to bind socket to any (all) network interfaces.
   */
  static final InetAddress ANY_IF = new InetAddress (zeros, null);
    
  private static final byte[] localhostAddress = { 127, 0, 0, 1 };

  private static InetAddress localhost = null;

  /**
   * The Serialized Form specifies that an int 'address' is saved/restored.
   * This class uses a byte array internally so we'll just do the conversion
   * at serialization time and leave the rest of the algorithm as is.
   */
  private int address;

  /**
   * An array of octets representing an IP address.
   */
  transient byte[] addr;

  /**
   * The name of the host for this address.
   */
  String hostName;
  
  /**
   * The field 'family' seems to be the AF_ value.
   * FIXME: Much of the code in the other java.net classes does not make
   * use of this family field.  A better implementation would be to make
   * use of getaddrinfo() and have other methods just check the family
   * field rather than examining the length of the address each time.
   */
  int family;

  /**
   * Initializes this object's addr instance variable from the passed in
   * int array.  Note that this constructor is protected and is called
   * only by static methods in this class.
   *
   * @param ipaddr The IP number of this address as an array of bytes
   */
  InetAddress (byte[] address)
  {
    this (address, null);
  }

  /**
   * Initializes this object's addr instance variable from the passed in
   * int array.  Note that this constructor is protected and is called
   * only by static methods in this class.
   *
   * @param ipaddr The IP number of this address as an array of bytes
   * @param hostname The hostname of this IP address.
   */
  InetAddress (byte[] address, String hostname)
  {
    addr = address;
    hostName = hostname;
    
    if (address != null)
      family = getFamily (address);
  }

  /**
   * Returns true if this address is a multicast address, false otherwise.
   * An address is multicast if the high four bits are "1110".  These are
   * also known as "Class D" addresses.
   *
   * @return true if mulitcast, false if not
   *
   * @since 1.1
   */
  public boolean isMulticastAddress ()
  {
    int len = addr.length;
    
    if (len == 4)
      return (addr [0] & 0xF0) == 0xE0;
    
    if (len == 16)
      return addr [0] == (byte) 0xFF;
    
    return false;
  }

  /**
   * Utility routine to check if the InetAddress in a wildcard address
   * 
   * @since 1.4
   */
  public boolean isAnyLocalAddress ()
  {
    // This is the IPv4 implementation.
    // Any class derived from InetAddress should override this.
    return addr == zeros;
  }

  /**
   * Utility routine to check if the InetAddress is a loopback address
   * 
   * @since 1.4
   */
  public boolean isLoopbackAddress ()
  {
    // This is the IPv4 implementation.
    // Any class derived from InetAddress should override this.
    
    return addr [0] == 0x7F;
  }

  /**
   * Utility routine to check if InetAddress is a link local address
   * 
   * @since 1.4
   */
  public boolean isLinkLocalAddress ()
  {
    // This is the IPv4 implementation.
    // Any class derived from InetAddress should override this.

    // XXX: This seems to not exist with IPv4 addresses
    return false;
  }

  /**
   * Utility routine to check if InetAddress is a site local address
   * 
   * @since 1.4
   */
  public boolean isSiteLocalAddress ()
  {
    // This is the IPv4 implementation.
    // Any class derived from InetAddress should override this.

    // 10.0.0.0/8
    if (addr [0] == 0x0A)
      return true;

    // XXX: Suns JDK 1.4.1 (on Linux) seems to have a bug here:
    // it says 172.16.0.0 - 172.255.255.255 are site local addresses

    // 172.16.0.0/12
    if (addr [0] == 0xAC
        && (addr [1] & 0xF0) == 0x01)
      return true;

    // 192.168.0.0/16
    if (addr [0] == 0xC0
        && addr [1] == 0xA8)
      return true;

    // XXX: Do we need to check more addresses here ?
    return false;
  }

  /**
   * Utility routine to check if InetAddress is a global multicast address
   * 
   * @since 1.4
   */
  public boolean isMCGlobal ()
  {
    // This is the IPv4 implementation.
    // Any class derived from InetAddress should override this.

    // XXX: This seems to not exist with IPv4 addresses
    return false;
  }

  /**
   * Utility reoutine to check if InetAddress is a node local multicast address
   * 
   * @since 1.4
   */
  public boolean isMCNodeLocal ()
  {
    // This is the IPv4 implementation.
    // Any class derived from InetAddress should override this.

    // XXX: This seems to not exist with IPv4 addresses
    return false;
  }

  /**
   * Utility reoutine to check if InetAddress is a link local multicast address
   * 
   * @since 1.4
   */
  public boolean isMCLinkLocal ()
  {
    // This is the IPv4 implementation.
    // Any class derived from InetAddress should override this.
    
    if (!isMulticastAddress ())
      return false;

    return (addr [0] == 0xE0
	    && addr [1] == 0x00
	    && addr [2] == 0x00);
  }

  /**
   * Utility routine to check if InetAddress is a site local multicast address
   *
   * @since 1.4
   */
  public boolean isMCSiteLocal ()
  {
    // This is the IPv4 implementation.
    // Any class derived from InetAddress should override this.

    // XXX: This seems to not exist with IPv4 addresses
    return false;
  }

  /**
   * Utility reoutine to check if InetAddress is a organization local
   * multicast address
   * 
   * @since 1.4
   */
  public boolean isMCOrgLocal ()
  {
    // This is the IPv4 implementation.
    // Any class derived from InetAddress should override this.

    // XXX: This seems to not exist with IPv4 addresses
    return false;
  }

  /**
   * Returns the hostname represented by this InetAddress
   */
  public String getHostName ()
  {
    if (hostName == null)
      lookup (null, this, false);

    return hostName;
  }

  /**
   * Returns the canonical hostname represented by this InetAddress
   * 
   * @since 1.4
   */
  public String getCanonicalHostName ()
  {
    SecurityManager sm = System.getSecurityManager ();
    if (sm != null)
      {
        try
	  {
            sm.checkConnect (hostName, -1);
	  }
	catch (SecurityException e)
	  {
	    return getHostAddress ();
	  }
      }

    // Try to find the FDQN now
    InetAddress address = new InetAddress (getAddress (), null);
    return address.getHostName ();
  }

  /**
   * Returns the IP address of this InetAddress as array of bytes
   */
  public byte[] getAddress ()
  {
    // An experiment shows that JDK1.2 returns a different byte array each
    // time.  This makes sense, in terms of security.
    return (byte[]) addr.clone ();
  }

  /* Helper function due to a CNI limitation.  */
  private static InetAddress[] allocArray (int count)
  {
    return new InetAddress [count];
  }

  /* Helper function due to a CNI limitation.  */
  private static SecurityException checkConnect (String hostname)
  {
    SecurityManager s = System.getSecurityManager ();
    
    if (s == null)
      return null;
    
    try
      {
	s.checkConnect (hostname, -1);
	return null;
      }
    catch (SecurityException ex)
      {
	return ex;
      }
  }

  /**
   * Returns the IP address as string
   *
   * @since 1.0.2
   */
  public String getHostAddress ()
  {
    StringBuffer sbuf = new StringBuffer (40);
    int len = addr.length;
    int i = 0;
    if (len == 16)
      { // An IPv6 address.
	for (;  ;  i += 2)
	  {
	    if (i >= 16)
	      return sbuf.toString ();
	    int x = ((addr [i] & 0xFF) << 8) | (addr [i + 1] & 0xFF);
	    boolean empty = sbuf.length () == 0;
	    if (empty)
	      {
		if (i == 10 && x == 0xFFFF)
		  { // IPv4-mapped IPv6 address.
		    sbuf.append (":FFFF:");
		    break;  // Continue as IPv4 address;
		  }
		else if (i == 12)
		  { // IPv4-compatible IPv6 address.
		    sbuf.append (':');
		    break;  // Continue as IPv4 address.
		  }
		else if (i > 0)
		  sbuf.append ("::");
	      }
	    else
	      sbuf.append (':');
	    if (x != 0 || i >= 14)
	      sbuf.append (Integer.toHexString (x).toUpperCase ());
	  }
      }
    for ( ;  ; )
      {
	sbuf.append (addr[i] & 0xFF);
	i++;
	if (i == len)
	  break;
	sbuf.append ('.');
      }
    
    return sbuf.toString();
  }

  /**
   * Returns a hashcode of the InetAddress
   */
  public int hashCode ()
  {
    // There hashing algorithm is not specified, but a simple experiment
    // shows that it is equal to the address, as a 32-bit big-endian integer.
    int hash = 0;
    int len = addr.length;
    int i = len > 4 ? len - 4 : 0;
    
    for ( ; i < len;  i++)
      hash = (hash << 8) | (addr[i] & 0xFF);
    
    return hash;
  }

  /**
   * Compares the InetAddress object with another one.
   */
  public boolean equals (Object obj)
  {
    if (obj == null
        || ! (obj instanceof InetAddress))
      return false;
    
    // "The Java Class Libraries" 2nd edition says "If a machine has
    // multiple names instances of InetAddress for different name of
    // that same machine are not equal.  This is because they have
    // different host names."  This violates the description in the
    // JDK 1.2 API documentation.  A little experimentation
    // shows that the latter is correct.
    byte[] addr1 = addr;
    byte[] addr2 = ((InetAddress) obj).addr;
    
    if (addr1.length != addr2.length)
      return false;
    
    for (int i = addr1.length;  --i >= 0;  )
      if (addr1[i] != addr2[i])
	return false;
    
    return true;
  }

  /**
   * Returns then <code>InetAddress</code> as string
   */
  public String toString ()
  {
    String result;
    String address = getHostAddress ();
    
    if (hostName != null)
      result = hostName + "/" + address;
    else
      result = address;
    
    return result;
  }

  /**
   * Returns an InetAddress object given the raw IP address.
   *
   * The argument is in network byte order: the highest order byte of the
   * address is in getAddress()[0].
   *
   * @param addr The IP address to create the InetAddress object from
   *
   * @exception UnknownHostException If IP address has illegal length
   *
   * @since 1.4
   */
  public static InetAddress getByAddress (byte[] addr)
    throws UnknownHostException
  {
    return getByAddress (null, addr);
  }

  /**
   * Creates an InetAddress based on the provided host name and IP address.
   * No name service is checked for the validity of the address.
   *
   * @param host The hostname of the InetAddress object to create
   * @param addr The IP address to create the InetAddress object from
   *
   * @exception UnknownHostException If IP address is of illegal length
   *
   * @since 1.4
   */
  public static InetAddress getByAddress (String host, byte[] addr)
    throws UnknownHostException
  {
    if (addr.length == 4)
      return new Inet4Address (addr, host);

    if (addr.length == 16)
      return new Inet6Address (addr, host);
    
    throw new UnknownHostException ("IP address has illegal length");
  }
  
  /**
   * If host is a valid numeric IP address, return the numeric address.
   * Otherwise, return null.
   */
  private static native byte[] aton (String host);

  private static native InetAddress[] lookup (String hostname,
		                              InetAddress addr, boolean all);

  private static native int getFamily (byte[] address);

  /**
   * Determines the IP address of a host, given the host's name.
   *
   * @exception UnknownHostException If no IP address for the host could
   * be found
   * @exception SecurityException If a security manager exists and its
   * checkConnect method doesn't allow the operation
   */
  public static InetAddress getByName (String hostname)
    throws UnknownHostException
  {
    SecurityManager s = System.getSecurityManager ();
    if (s != null)
      s.checkConnect (hostname, -1);
   
    // Default to current host if necessary
    if (hostname == null)
      return getLocalHost ();

    // Assume that the host string is an IP address
    byte[] address = aton (hostname);
    if (address != null)
      {
        if (address.length == 4)
          return new Inet4Address (address, null);
        else if (address.length == 16)
          {
	    if ((address [10] == 0xFF) && (address [11] == 0xFF))
	      {
		byte[] ip4addr = new byte [4];
		ip4addr [0] = address [12];
		ip4addr [1] = address [13];
		ip4addr [2] = address [14];
		ip4addr [3] = address [15];
		return new Inet4Address (ip4addr, null);
	      }
            return new Inet6Address (address, null);
	  }
	else
          throw new UnknownHostException ("Address has invalid length");
      }
   
    // Try to resolve the host by DNS
    InetAddress[] addresses = getAllByName (hostname);
    return addresses [0];
  }

  /**
   * Given the name of a host, returns an array of its IP addresses,
   * based on the configured name service on the system.
   *
   * @exception UnknownHostException If no IP address for the host could
   * be found
   * @exception SecurityException If a security manager exists and its
   * checkConnect method doesn't allow the operation
   */
  public static InetAddress[] getAllByName (String hostname)
    throws UnknownHostException
  {
    SecurityManager s = System.getSecurityManager ();
    if (s != null)
      s.checkConnect (hostname, -1);

    // Check if hostname is an IP address
    byte[] address = aton (hostname);
    if (address != null)
      {
	InetAddress[] result = new InetAddress [1];
	result [0] = new InetAddress (address, null);
	return result;
      }
   
    // Try to resolve the hostname by DNS
    return lookup (hostname, null, true);
  }

  private static native String getLocalHostname ();

  /**
   * Returns the local host
   *
   * @exception UnknownHostException If no IP address for the host could
   * be found
   */
  public static InetAddress getLocalHost () throws UnknownHostException
  {
    SecurityManager s = System.getSecurityManager ();
    
    // Experimentation shows that JDK1.2 does cache the result.
    // However, if there is a security manager, and the cached result
    // is other than "localhost", we need to check again.
    if (localhost == null
	|| (s != null && localhost.addr != localhostAddress))
      getLocalHost (s);
    
    return localhost;
  }

  private static synchronized void getLocalHost (SecurityManager s)
    throws UnknownHostException
  {
    // Check the localhost cache again, now that we've synchronized.
    if (s == null && localhost != null)
      return;
    
    String hostname = getLocalHostname ();
    
    if (s != null)
      {
	// "The Java Class Libraries" suggests that if the security
	// manager disallows getting the local host name, then
	// we use the loopback host.
	// However, the JDK 1.2 API claims to throw SecurityException,
	// which seems to suggest SecurityException is *not* caught.
	// In this case, experimentation shows that former is correct.
	try
	  {
	    // This is wrong, if the name returned from getLocalHostname()
	    // is not a fully qualified name.  FIXME.
	    s.checkConnect (hostname, -1);
	  }
	catch (SecurityException ex)
	  {
	    hostname = null;
	  }
      }
    
    if (hostname != null)
      {
	try
	  {
	    localhost = new InetAddress (null, null);
	    lookup (hostname, localhost, false);
	  }
	catch (Exception ex)
	  {
	  }
      }
    
    if (localhost == null)
      localhost = new InetAddress (localhostAddress, "localhost");
  }

  /**
   * Needed for serialization
   */
  private void readResolve () throws ObjectStreamException
  {
    // FIXME: implement this
  }
	  
  private void readObject (ObjectInputStream ois)
    throws IOException, ClassNotFoundException
  {
    ois.defaultReadObject ();
    addr = new byte [4];
    addr [3] = (byte) address;
    
    for (int i = 2; i >= 0; --i)
      addr [i] = (byte) (address >>= 8);
    
    // Ignore family from serialized data.  Since the saved address is 32 bits
    // the deserialized object will have an IPv4 address i.e. AF_INET family.
    // FIXME: An alternative is to call the aton method on the deserialized
    // hostname to get a new address.  The Serialized Form doc is silent
    // on how these fields are used.
    family = getFamily (addr);
  }

  private void writeObject (ObjectOutputStream oos) throws IOException
  {
    // Build a 32 bit address from the last 4 bytes of a 4 byte IPv4 address
    // or a 16 byte IPv6 address.
    int len = addr.length;
    int i = len - 4;
    
    for (; i < len; i++)
      address = address << 8 | (((int) addr [i]) & 0xFF);
    
    oos.defaultWriteObject ();
  }
}
