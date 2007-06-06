/****************************************************************************
 *                                                                          *
 *                         GNAT COMPILER COMPONENTS                         *
 *                                                                          *
 *                               S N A M E S                                *
 *                                                                          *
 *                              C Header File                               *
 *                                                                          *
 *          Copyright (C) 1992-2007, Free Software Foundation, Inc.         *
 *                                                                          *
 * GNAT is free software;  you can  redistribute it  and/or modify it under *
 * terms of the  GNU General Public License as published  by the Free Soft- *
 * ware  Foundation;  either version 2,  or (at your option) any later ver- *
 * sion.  GNAT is distributed in the hope that it will be useful, but WITH- *
 * OUT ANY WARRANTY;  without even the  implied warranty of MERCHANTABILITY *
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License *
 * for  more details.  You should have  received  a copy of the GNU General *
 * Public License  distributed with GNAT;  see file COPYING.  If not, write *
 * to  the  Free Software Foundation,  51  Franklin  Street,  Fifth  Floor, *
 * Boston, MA 02110-1301, USA.                                              *
 *                                                                          *
 * GNAT was originally developed  by the GNAT team at  New York University. *
 * Extensive contributions were provided by Ada Core Technologies Inc.      *
 *                                                                          *
 ****************************************************************************/

/* This is the C file that corresponds to the Ada package specification
   Snames. It was created automatically from the file snames.ads. */

/* Name_Id values */

#define Name_uParent    (First_Name_Id + 256 + 0)
#define Name_uTag       (First_Name_Id + 256 + 1)
#define Name_Off        (First_Name_Id + 256 + 2)
#define Name_Space      (First_Name_Id + 256 + 3)
#define Name_Time       (First_Name_Id + 256 + 4)

/* Define the function to return one of the numeric values below. Note
   that it actually returns a char since an enumeration value of less
   than 256 entries is represented that way in Ada.  The operand is a Chars
   field value.  */

#define Get_Attribute_Id snames__get_attribute_id
extern unsigned char Get_Attribute_Id (int);

/* Define the numeric values for attributes.  */

#define  Attr_Abort_Signal                  0
#define  Attr_Access                        1
#define  Attr_Address                       2
#define  Attr_Address_Size                  3
#define  Attr_Aft                           4
#define  Attr_Alignment                     5
#define  Attr_Asm_Input                     6
#define  Attr_Asm_Output                    7
#define  Attr_AST_Entry                     8
#define  Attr_Bit                           9
#define  Attr_Bit_Order                     10
#define  Attr_Bit_Position                  11
#define  Attr_Body_Version                  12
#define  Attr_Callable                      13
#define  Attr_Caller                        14
#define  Attr_Code_Address                  15
#define  Attr_Component_Size                16
#define  Attr_Compose                       17
#define  Attr_Constrained                   18
#define  Attr_Count                         19
#define  Attr_Default_Bit_Order             20
#define  Attr_Definite                      21
#define  Attr_Delta                         22
#define  Attr_Denorm                        23
#define  Attr_Digits                        24
#define  Attr_Elaborated                    25
#define  Attr_Emax                          26
#define  Attr_Enum_Rep                      27
#define  Attr_Epsilon                       28
#define  Attr_Exponent                      29
#define  Attr_External_Tag                  30
#define  Attr_First                         31
#define  Attr_First_Bit                     32
#define  Attr_Fixed_Value                   33
#define  Attr_Fore                          34
#define  Attr_Has_Access_Values             35
#define  Attr_Has_Discriminants             36
#define  Attr_Identity                      37
#define  Attr_Img                           38
#define  Attr_Integer_Value                 39
#define  Attr_Large                         40
#define  Attr_Last                          41
#define  Attr_Last_Bit                      42
#define  Attr_Leading_Part                  43
#define  Attr_Length                        44
#define  Attr_Machine_Emax                  45
#define  Attr_Machine_Emin                  46
#define  Attr_Machine_Mantissa              47
#define  Attr_Machine_Overflows             48
#define  Attr_Machine_Radix                 49
#define  Attr_Machine_Rounding              50
#define  Attr_Machine_Rounds                51
#define  Attr_Machine_Size                  52
#define  Attr_Mantissa                      53
#define  Attr_Max_Size_In_Storage_Elements  54
#define  Attr_Maximum_Alignment             55
#define  Attr_Mechanism_Code                56
#define  Attr_Mod                           57
#define  Attr_Model_Emin                    58
#define  Attr_Model_Epsilon                 59
#define  Attr_Model_Mantissa                60
#define  Attr_Model_Small                   61
#define  Attr_Modulus                       62
#define  Attr_Null_Parameter                63
#define  Attr_Object_Size                   64
#define  Attr_Partition_ID                  65
#define  Attr_Passed_By_Reference           66
#define  Attr_Pool_Address                  67
#define  Attr_Pos                           68
#define  Attr_Position                      69
#define  Attr_Priority                      70
#define  Attr_Range                         71
#define  Attr_Range_Length                  72
#define  Attr_Round                         73
#define  Attr_Safe_Emax                     74
#define  Attr_Safe_First                    75
#define  Attr_Safe_Large                    76
#define  Attr_Safe_Last                     77
#define  Attr_Safe_Small                    78
#define  Attr_Scale                         79
#define  Attr_Scaling                       80
#define  Attr_Signed_Zeros                  81
#define  Attr_Size                          82
#define  Attr_Small                         83
#define  Attr_Storage_Size                  84
#define  Attr_Storage_Unit                  85
#define  Attr_Stream_Size                   86
#define  Attr_Tag                           87
#define  Attr_Target_Name                   88
#define  Attr_Terminated                    89
#define  Attr_To_Address                    90
#define  Attr_Type_Class                    91
#define  Attr_UET_Address                   92
#define  Attr_Unbiased_Rounding             93
#define  Attr_Unchecked_Access              94
#define  Attr_Unconstrained_Array           95
#define  Attr_Universal_Literal_String      96
#define  Attr_Unrestricted_Access           97
#define  Attr_VADS_Size                     98
#define  Attr_Val                           99
#define  Attr_Valid                         100
#define  Attr_Value_Size                    101
#define  Attr_Version                       102
#define  Attr_Wchar_T_Size                  103
#define  Attr_Wide_Wide_Width               104
#define  Attr_Wide_Width                    105
#define  Attr_Width                         106
#define  Attr_Word_Size                     107
#define  Attr_Adjacent                      108
#define  Attr_Ceiling                       109
#define  Attr_Copy_Sign                     110
#define  Attr_Floor                         111
#define  Attr_Fraction                      112
#define  Attr_Image                         113
#define  Attr_Input                         114
#define  Attr_Machine                       115
#define  Attr_Max                           116
#define  Attr_Min                           117
#define  Attr_Model                         118
#define  Attr_Pred                          119
#define  Attr_Remainder                     120
#define  Attr_Rounding                      121
#define  Attr_Succ                          122
#define  Attr_Truncation                    123
#define  Attr_Value                         124
#define  Attr_Wide_Image                    125
#define  Attr_Wide_Wide_Image               126
#define  Attr_Wide_Value                    127
#define  Attr_Wide_Wide_Value               128
#define  Attr_Output                        129
#define  Attr_Read                          130
#define  Attr_Write                         131
#define  Attr_Elab_Body                     132
#define  Attr_Elab_Spec                     133
#define  Attr_Storage_Pool                  134
#define  Attr_Base                          135
#define  Attr_Class                         136
#define  Attr_Stub_Type                     137

/* Define the numeric values for the conventions.  */

#define  Convention_Ada                           0
#define  Convention_Intrinsic                     1
#define  Convention_Entry                         2
#define  Convention_Protected                     3
#define  Convention_Assembler                     4
#define  Convention_C                             5
#define  Convention_CIL                           6
#define  Convention_COBOL                         7
#define  Convention_CPP                           8
#define  Convention_Fortran                       9
#define  Convention_Java                          10
#define  Convention_Stdcall                       11
#define  Convention_Stubbed                       12

/* Define the function to check if a Name_Id value is a valid pragma */

#define Is_Pragma_Name snames__is_pragma_name
extern Boolean Is_Pragma_Name (Name_Id);

/* Define the function to return one of the numeric values below.  Note
   that it actually returns a char since an enumeration value of less
   than 256 entries is represented that way in Ada.  The operand is a Chars
   field value.  */

#define Get_Pragma_Id snames__get_pragma_id
extern unsigned char Get_Pragma_Id (int);

/* Define the numeric values for the pragmas. */

#define  Pragma_Ada_83                        0
#define  Pragma_Ada_95                        1
#define  Pragma_Ada_05                        2
#define  Pragma_Ada_2005                      3
#define  Pragma_Assertion_Policy              4
#define  Pragma_C_Pass_By_Copy                5
#define  Pragma_Compile_Time_Error            6
#define  Pragma_Compile_Time_Warning          7
#define  Pragma_Component_Alignment           8
#define  Pragma_Convention_Identifier         9
#define  Pragma_Debug_Policy                  10
#define  Pragma_Detect_Blocking               11
#define  Pragma_Discard_Names                 12
#define  Pragma_Elaboration_Checks            13
#define  Pragma_Eliminate                     14
#define  Pragma_Extend_System                 15
#define  Pragma_Extensions_Allowed            16
#define  Pragma_External_Name_Casing          17
#define  Pragma_Float_Representation          18
#define  Pragma_Initialize_Scalars            19
#define  Pragma_Interrupt_State               20
#define  Pragma_License                       21
#define  Pragma_Locking_Policy                22
#define  Pragma_Long_Float                    23
#define  Pragma_No_Run_Time                   24
#define  Pragma_No_Strict_Aliasing            25
#define  Pragma_Normalize_Scalars             26
#define  Pragma_Polling                       27
#define  Pragma_Persistent_BSS                28
#define  Pragma_Priority_Specific_Dispatching 29
#define  Pragma_Profile                       30
#define  Pragma_Profile_Warnings              31
#define  Pragma_Propagate_Exceptions          32
#define  Pragma_Queuing_Policy                33
#define  Pragma_Ravenscar                     34
#define  Pragma_Restricted_Run_Time           35
#define  Pragma_Restrictions                  36
#define  Pragma_Restriction_Warnings          37
#define  Pragma_Reviewable                    38
#define  Pragma_Source_File_Name              39
#define  Pragma_Source_File_Name_Project      40
#define  Pragma_Style_Checks                  41
#define  Pragma_Suppress                      42
#define  Pragma_Suppress_Exception_Locations  43
#define  Pragma_Task_Dispatching_Policy       44
#define  Pragma_Universal_Data                45
#define  Pragma_Unsuppress                    46
#define  Pragma_Use_VADS_Size                 47
#define  Pragma_Validity_Checks               48
#define  Pragma_Warnings                      49
#define  Pragma_Wide_Character_Encoding       50
#define  Pragma_Abort_Defer                   51
#define  Pragma_All_Calls_Remote              52
#define  Pragma_Annotate                      53
#define  Pragma_Assert                        54
#define  Pragma_Asynchronous                  55
#define  Pragma_Atomic                        56
#define  Pragma_Atomic_Components             57
#define  Pragma_Attach_Handler                58
#define  Pragma_CIL_Constructor               59
#define  Pragma_Comment                       60
#define  Pragma_Common_Object                 61
#define  Pragma_Complete_Representation       62
#define  Pragma_Complex_Representation        63
#define  Pragma_Controlled                    64
#define  Pragma_Convention                    65
#define  Pragma_CPP_Class                     66
#define  Pragma_CPP_Constructor               67
#define  Pragma_CPP_Virtual                   68
#define  Pragma_CPP_Vtable                    69
#define  Pragma_Debug                         70
#define  Pragma_Elaborate                     71
#define  Pragma_Elaborate_All                 72
#define  Pragma_Elaborate_Body                73
#define  Pragma_Export                        74
#define  Pragma_Export_Exception              75
#define  Pragma_Export_Function               76
#define  Pragma_Export_Object                 77
#define  Pragma_Export_Procedure              78
#define  Pragma_Export_Value                  79
#define  Pragma_Export_Valued_Procedure       80
#define  Pragma_External                      81
#define  Pragma_Finalize_Storage_Only         82
#define  Pragma_Ident                         83
#define  Pragma_Import                        84
#define  Pragma_Import_Exception              85
#define  Pragma_Import_Function               86
#define  Pragma_Import_Object                 87
#define  Pragma_Import_Procedure              88
#define  Pragma_Import_Valued_Procedure       89
#define  Pragma_Inline                        90
#define  Pragma_Inline_Always                 91
#define  Pragma_Inline_Generic                92
#define  Pragma_Inspection_Point              93
#define  Pragma_Interface_Name                94
#define  Pragma_Interrupt_Handler             95
#define  Pragma_Interrupt_Priority            96
#define  Pragma_Java_Constructor              97
#define  Pragma_Java_Interface                98
#define  Pragma_Keep_Names                    99
#define  Pragma_Link_With                     100
#define  Pragma_Linker_Alias                  101
#define  Pragma_Linker_Constructor            102
#define  Pragma_Linker_Destructor             103
#define  Pragma_Linker_Options                104
#define  Pragma_Linker_Section                105
#define  Pragma_List                          106
#define  Pragma_Machine_Attribute             107
#define  Pragma_Main                          108
#define  Pragma_Main_Storage                  109
#define  Pragma_Memory_Size                   110
#define  Pragma_No_Body                       111
#define  Pragma_No_Return                     112
#define  Pragma_Obsolescent                   113
#define  Pragma_Optimize                      114
#define  Pragma_Pack                          115
#define  Pragma_Page                          116
#define  Pragma_Passive                       117
#define  Pragma_Preelaborable_Initialization  118
#define  Pragma_Preelaborate                  119
#define  Pragma_Preelaborate_05               120
#define  Pragma_Psect_Object                  121
#define  Pragma_Pure                          122
#define  Pragma_Pure_05                       123
#define  Pragma_Pure_Function                 124
#define  Pragma_Remote_Call_Interface         125
#define  Pragma_Remote_Types                  126
#define  Pragma_Share_Generic                 127
#define  Pragma_Shared                        128
#define  Pragma_Shared_Passive                129
#define  Pragma_Source_Reference              130
#define  Pragma_Static_Elaboration_Desired    131
#define  Pragma_Stream_Convert                132
#define  Pragma_Subtitle                      133
#define  Pragma_Suppress_All                  134
#define  Pragma_Suppress_Debug_Info           135
#define  Pragma_Suppress_Initialization       136
#define  Pragma_System_Name                   137
#define  Pragma_Task_Info                     138
#define  Pragma_Task_Name                     139
#define  Pragma_Task_Storage                  140
#define  Pragma_Time_Slice                    141
#define  Pragma_Title                         142
#define  Pragma_Unchecked_Union               143
#define  Pragma_Unimplemented_Unit            144
#define  Pragma_Universal_Aliasing            145
#define  Pragma_Unreferenced                  146
#define  Pragma_Unreferenced_Objects          147
#define  Pragma_Unreserve_All_Interrupts      148
#define  Pragma_Volatile                      149
#define  Pragma_Volatile_Components           150
#define  Pragma_Weak_External                 151
#define  Pragma_AST_Entry                     152
#define  Pragma_Interface                     153
#define  Pragma_Priority                      154
#define  Pragma_Storage_Size                  155
#define  Pragma_Storage_Unit                  156

/* End of snames.h (C version of Snames package spec) */
