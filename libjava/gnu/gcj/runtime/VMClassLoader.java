/* Copyright (C) 1999, 2001, 2002, 2003, 2004, 2005  Free Software Foundation

   This file is part of libgcj.

This software is copyrighted work licensed under the terms of the
Libgcj License.  Please consult the file "LIBGCJ_LICENSE" for
details.  */

/* Author: Kresten Krab Thorup <krab@gnu.org>  */

package gnu.gcj.runtime;

import java.net.URL;
import java.util.HashSet;

// Despite its name, this class is really the extension loader for
// libgcj.  Class loader bootstrap is a bit tricky, see prims.cc and
// SystemClassLoader for some details.
public final class VMClassLoader extends HelperClassLoader
{
  private VMClassLoader ()
  {	
    String p
      = System.getProperty ("gnu.gcj.runtime.VMClassLoader.library_control",
			    "");
    if ("never".equals(p))
      lib_control = LIB_NEVER;
    else if ("cache".equals(p))
      lib_control = LIB_CACHE;
    else if ("full".equals(p))
      lib_control = LIB_FULL;
    else
      lib_control = LIB_CACHE;
  }

  private void init() 
  {
    addDirectoriesFromProperty("java.ext.dirs");
  }

  /** This is overridden to search the internal hash table, which 
   * will only search existing linked-in classes.   This will make
   * the default implementation of loadClass (in ClassLoader) work right.
   * The implementation of this method is in
   * gnu/gcj/runtime/natVMClassLoader.cc.
   */
  protected native Class findClass(String name) 
    throws java.lang.ClassNotFoundException;

  // This can be package-private because we only call it from native
  // code during startup.
  static void initialize ()
  {
    instance.init();
    system_instance.init();
  }

  // Define a package for something loaded natively.
  void definePackageForNative(String className)
  {
    int lastDot = className.lastIndexOf('.');
    if (lastDot != -1)
      {
	String packageName = className.substring(0, lastDot);
	if (getPackage(packageName) == null)
	  {
	    // FIXME: this assumes we're defining the core, which
	    // isn't necessarily so.  We could detect this and set up
	    // appropriately.  We could also look at a manifest file
	    // compiled into the .so.
	    definePackage(packageName, "Java Platform API Specification",
			  "GNU", "1.4", "gcj", "GNU",
			  null, // FIXME: gcj version.
			  null);
	  }
      }
  }

  // This keeps track of shared libraries we've already tried to load.
  private HashSet tried_libraries = new HashSet();

  // Holds one of the LIB_* constants; used to determine how shared
  // library loads are done.
  private int lib_control;

  // The only VMClassLoader that can exist.
  static VMClassLoader instance = new VMClassLoader();
  // The system class loader.
  static SystemClassLoader system_instance = new SystemClassLoader(instance);

  private static final int LIB_FULL = 0;
  private static final int LIB_CACHE = 1;
  private static final int LIB_NEVER = 2;
}
