------------------------------------------------------------------------------
--                                                                          --
--                         GNAT COMPILER COMPONENTS                         --
--                                                                          --
--                             E X P _ D I S P                              --
--                                                                          --
--                                 B o d y                                  --
--                                                                          --
--          Copyright (C) 1992-2005 Free Software Foundation, Inc.          --
--                                                                          --
-- GNAT is free software;  you can  redistribute it  and/or modify it under --
-- terms of the  GNU General Public License as published  by the Free Soft- --
-- ware  Foundation;  either version 2,  or (at your option) any later ver- --
-- sion.  GNAT is distributed in the hope that it will be useful, but WITH- --
-- OUT ANY WARRANTY;  without even the  implied warranty of MERCHANTABILITY --
-- or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License --
-- for  more details.  You should have  received  a copy of the GNU General --
-- Public License  distributed with GNAT;  see file COPYING.  If not, write --
-- to  the Free Software Foundation,  59 Temple Place - Suite 330,  Boston, --
-- MA 02111-1307, USA.                                                      --
--                                                                          --
-- GNAT was originally developed  by the GNAT team at  New York University. --
-- Extensive contributions were provided by Ada Core Technologies Inc.      --
--                                                                          --
------------------------------------------------------------------------------

with Atree;    use Atree;
with Checks;   use Checks;
with Debug;    use Debug;
with Einfo;    use Einfo;
with Elists;   use Elists;
with Errout;   use Errout;
with Exp_Ch7;  use Exp_Ch7;
with Exp_Tss;  use Exp_Tss;
with Exp_Util; use Exp_Util;
with Itypes;   use Itypes;
with Nlists;   use Nlists;
with Nmake;    use Nmake;
with Namet;    use Namet;
with Opt;      use Opt;
with Output;   use Output;
with Rtsfind;  use Rtsfind;
with Sem;      use Sem;
with Sem_Disp; use Sem_Disp;
with Sem_Res;  use Sem_Res;
with Sem_Type; use Sem_Type;
with Sem_Util; use Sem_Util;
with Sinfo;    use Sinfo;
with Snames;   use Snames;
with Stand;    use Stand;
with Tbuild;   use Tbuild;
with Ttypes;   use Ttypes;
with Uintp;    use Uintp;

package body Exp_Disp is

   Ada_Actions : constant array (DT_Access_Action) of RE_Id :=
      (CW_Membership           => RE_CW_Membership,
       IW_Membership           => RE_IW_Membership,
       DT_Entry_Size           => RE_DT_Entry_Size,
       DT_Prologue_Size        => RE_DT_Prologue_Size,
       Get_Access_Level        => RE_Get_Access_Level,
       Get_External_Tag        => RE_Get_External_Tag,
       Get_Prim_Op_Address     => RE_Get_Prim_Op_Address,
       Get_RC_Offset           => RE_Get_RC_Offset,
       Get_Remotely_Callable   => RE_Get_Remotely_Callable,
       Inherit_DT              => RE_Inherit_DT,
       Inherit_TSD             => RE_Inherit_TSD,
       Register_Interface_Tag  => RE_Register_Interface_Tag,
       Register_Tag            => RE_Register_Tag,
       Set_Access_Level        => RE_Set_Access_Level,
       Set_Expanded_Name       => RE_Set_Expanded_Name,
       Set_External_Tag        => RE_Set_External_Tag,
       Set_Prim_Op_Address     => RE_Set_Prim_Op_Address,
       Set_RC_Offset           => RE_Set_RC_Offset,
       Set_Remotely_Callable   => RE_Set_Remotely_Callable,
       Set_TSD                 => RE_Set_TSD,
       TSD_Entry_Size          => RE_TSD_Entry_Size,
       TSD_Prologue_Size       => RE_TSD_Prologue_Size);

   Action_Is_Proc : constant array (DT_Access_Action) of Boolean :=
      (CW_Membership           => False,
       IW_Membership           => False,
       DT_Entry_Size           => False,
       DT_Prologue_Size        => False,
       Get_Access_Level        => False,
       Get_External_Tag        => False,
       Get_Prim_Op_Address     => False,
       Get_Remotely_Callable   => False,
       Get_RC_Offset           => False,
       Inherit_DT              => True,
       Inherit_TSD             => True,
       Register_Interface_Tag  => True,
       Register_Tag            => True,
       Set_Access_Level        => True,
       Set_Expanded_Name       => True,
       Set_External_Tag        => True,
       Set_Prim_Op_Address     => True,
       Set_RC_Offset           => True,
       Set_Remotely_Callable   => True,
       Set_TSD                 => True,
       TSD_Entry_Size          => False,
       TSD_Prologue_Size       => False);

   Action_Nb_Arg : constant array (DT_Access_Action) of Int :=
      (CW_Membership           => 2,
       IW_Membership           => 2,
       DT_Entry_Size           => 0,
       DT_Prologue_Size        => 0,
       Get_Access_Level        => 1,
       Get_External_Tag        => 1,
       Get_Prim_Op_Address     => 2,
       Get_RC_Offset           => 1,
       Get_Remotely_Callable   => 1,
       Inherit_DT              => 3,
       Inherit_TSD             => 2,
       Register_Interface_Tag  => 2,
       Register_Tag            => 1,
       Set_Access_Level        => 2,
       Set_Expanded_Name       => 2,
       Set_External_Tag        => 2,
       Set_Prim_Op_Address     => 3,
       Set_RC_Offset           => 2,
       Set_Remotely_Callable   => 2,
       Set_TSD                 => 2,
       TSD_Entry_Size          => 0,
       TSD_Prologue_Size       => 0);

   function Build_Anonymous_Access_Type
     (Directly_Designated_Type : Entity_Id;
      Related_Nod              : Node_Id) return Entity_Id;
   --  Returns a decorated entity corresponding with an anonymous access type.
   --  Used to generate unchecked type conversion of an address.

   procedure Collect_All_Interfaces (T : Entity_Id);
   --  Ada 2005 (AI-251): Collect the whole list of interfaces that are
   --  directly or indirectly implemented by T. Used to compute the size
   --  of the table of interfaces.

   function Default_Prim_Op_Position (Subp : Entity_Id) return Uint;
   --  Ada 2005 (AI-251): Returns the fixed position in the dispatch table
   --  of the default primitive operations.

   function Original_View_In_Visible_Part (Typ : Entity_Id) return Boolean;
   --  Check if the type has a private view or if the public view appears
   --  in the visible part of a package spec.

   ----------------------------------
   --  Build_Anonymous_Access_Type --
   ----------------------------------

   function Build_Anonymous_Access_Type
     (Directly_Designated_Type : Entity_Id;
      Related_Nod              : Node_Id) return Entity_Id
   is
      New_E : Entity_Id;

   begin
      New_E := Create_Itype (Ekind       => E_Anonymous_Access_Type,
                             Related_Nod => Related_Nod,
                             Scope_Id    => Current_Scope);

      Set_Etype                    (New_E, New_E);
      Init_Size_Align              (New_E);
      Init_Size                    (New_E, System_Address_Size);
      Set_Directly_Designated_Type (New_E, Directly_Designated_Type);
      Set_Is_First_Subtype         (New_E);

      return New_E;
   end Build_Anonymous_Access_Type;

   ----------------------------
   -- Collect_All_Interfaces --
   ----------------------------

   procedure Collect_All_Interfaces (T : Entity_Id) is

      procedure Add_Interface (Iface : Entity_Id);
      --  Add the interface it if is not already in the list

      procedure Collect (Typ   : Entity_Id);
      --  Subsidiary subprogram used to traverse the whole list
      --  of directly and indirectly implemented interfaces

      -------------------
      -- Add_Interface --
      -------------------

      procedure Add_Interface (Iface : Entity_Id) is
         Elmt  : Elmt_Id := First_Elmt (Abstract_Interfaces (T));

      begin
         while Present (Elmt) and then Node (Elmt) /= Iface loop
            Next_Elmt (Elmt);
         end loop;

         if not Present (Elmt) then
            Append_Elmt (Iface, Abstract_Interfaces (T));
         end if;
      end Add_Interface;

      -------------
      -- Collect --
      -------------

      procedure Collect (Typ : Entity_Id) is
         Nod      : constant Node_Id := Type_Definition (Parent (Typ));
         Id       : Node_Id;
         Iface    : Entity_Id;
         Ancestor : Entity_Id;

      begin
         pragma Assert (False
            or else Nkind (Nod) = N_Derived_Type_Definition
            or else Nkind (Nod) = N_Record_Definition);

         if Nkind (Nod) = N_Record_Definition then
            return;
         end if;

         --  Include the ancestor if we are generating the whole list
         --  of interfaces. This is used to know the size of the table
         --  that stores the tag of all the ancestor interfaces.

         Ancestor := Etype (Typ);

         if Is_Interface (Ancestor) then
            Add_Interface (Ancestor);
         end if;

         if Ancestor /= Typ
           and then Ekind (Ancestor) /= E_Record_Type_With_Private
         then
            Collect (Ancestor);
         end if;

         --  Traverse the graph of ancestor interfaces

         if Is_Non_Empty_List (Interface_List (Nod)) then
            Id := First (Interface_List (Nod));

            while Present (Id) loop

               Iface := Etype (Id);

               if Is_Interface (Iface) then
                  Add_Interface (Iface);
                  Collect (Iface);
               end if;

               Next (Id);
            end loop;
         end if;
      end Collect;

   --  Start of processing for Collect_All_Interfaces

   begin
      Collect (T);
   end Collect_All_Interfaces;

   ------------------------------
   -- Default_Prim_Op_Position --
   ------------------------------

   function Default_Prim_Op_Position (Subp : Entity_Id) return Uint is
      TSS_Name : TSS_Name_Type;
      E        : Entity_Id := Subp;

   begin
      --  Handle overriden subprograms

      while Present (Alias (E)) loop
         E := Alias (E);
      end loop;

      Get_Name_String (Chars (E));
      TSS_Name :=
        TSS_Name_Type
          (Name_Buffer (Name_Len - TSS_Name'Length + 1 .. Name_Len));

      if Chars (E) = Name_uSize then
         return Uint_1;

      elsif Chars (E) = Name_uAlignment then
         return Uint_2;

      elsif TSS_Name = TSS_Stream_Read then
         return Uint_3;

      elsif TSS_Name = TSS_Stream_Write then
         return Uint_4;

      elsif TSS_Name = TSS_Stream_Input then
         return Uint_5;

      elsif TSS_Name = TSS_Stream_Output then
         return Uint_6;

      elsif Chars (E) = Name_Op_Eq then
         return Uint_7;

      elsif Chars (E) = Name_uAssign then
         return Uint_8;

      elsif TSS_Name = TSS_Deep_Adjust then
         return Uint_9;

      elsif TSS_Name = TSS_Deep_Finalize then
         return Uint_10;

      else
         raise Program_Error;
      end if;
   end Default_Prim_Op_Position;

   -----------------------------
   -- Expand_Dispatching_Call --
   -----------------------------

   procedure Expand_Dispatching_Call (Call_Node : Node_Id) is
      Loc      : constant Source_Ptr := Sloc (Call_Node);
      Call_Typ : constant Entity_Id  := Etype (Call_Node);

      Ctrl_Arg   : constant Node_Id := Controlling_Argument (Call_Node);
      Param_List : constant List_Id := Parameter_Associations (Call_Node);
      Subp       : Entity_Id        := Entity (Name (Call_Node));

      CW_Typ          : Entity_Id;
      New_Call        : Node_Id;
      New_Call_Name   : Node_Id;
      New_Params      : List_Id := No_List;
      Param           : Node_Id;
      Res_Typ         : Entity_Id;
      Subp_Ptr_Typ    : Entity_Id;
      Subp_Typ        : Entity_Id;
      Typ             : Entity_Id;
      Eq_Prim_Op      : Entity_Id := Empty;
      Controlling_Tag : Node_Id;

      function New_Value (From : Node_Id) return Node_Id;
      --  From is the original Expression. New_Value is equivalent to a call
      --  to Duplicate_Subexpr with an explicit dereference when From is an
      --  access parameter.

      function Controlling_Type (Subp : Entity_Id) return Entity_Id;
      --  Returns the tagged type for which Subp is a primitive subprogram

      ---------------
      -- New_Value --
      ---------------

      function New_Value (From : Node_Id) return Node_Id is
         Res : constant Node_Id := Duplicate_Subexpr (From);
      begin
         if Is_Access_Type (Etype (From)) then
            return Make_Explicit_Dereference (Sloc (From), Res);
         else
            return Res;
         end if;
      end New_Value;

      ----------------------
      -- Controlling_Type --
      ----------------------

      function Controlling_Type (Subp : Entity_Id) return Entity_Id is
      begin
         if Ekind (Subp) = E_Function
           and then Has_Controlling_Result (Subp)
         then
            return Base_Type (Etype (Subp));

         else
            declare
               Formal : Entity_Id := First_Formal (Subp);

            begin
               while Present (Formal) loop
                  if Is_Controlling_Formal (Formal) then
                     if Is_Access_Type (Etype (Formal)) then
                        return Base_Type (Designated_Type (Etype (Formal)));
                     else
                        return Base_Type (Etype (Formal));
                     end if;
                  end if;

                  Next_Formal (Formal);
               end loop;
            end;
         end if;

         --  Controlling type not found (should never happen)

         return Empty;
      end Controlling_Type;

   --  Start of processing for Expand_Dispatching_Call

   begin
      --  If this is an inherited operation that was overridden, the body
      --  that is being called is its alias.

      if Present (Alias (Subp))
        and then Is_Inherited_Operation (Subp)
        and then No (DTC_Entity (Subp))
      then
         Subp := Alias (Subp);
      end if;

      --  Expand_Dispatching_Call is called directly from the semantics,
      --  so we need a check to see whether expansion is active before
      --  proceeding.

      if not Expander_Active then
         return;
      end if;

      --  Definition of the class-wide type and the tagged type

      --  If the controlling argument is itself a tag rather than a tagged
      --  object, then use the class-wide type associated with the subprogram's
      --  controlling type. This case can occur when a call to an inherited
      --  primitive has an actual that originated from a default parameter
      --  given by a tag-indeterminate call and when there is no other
      --  controlling argument providing the tag (AI-239 requires dispatching).
      --  This capability of dispatching directly by tag is also needed by the
      --  implementation of AI-260 (for the generic dispatching constructors).

      if Etype (Ctrl_Arg) = RTE (RE_Tag)
        or else Etype (Ctrl_Arg) = RTE (RE_Interface_Tag)
      then
         CW_Typ := Class_Wide_Type (Controlling_Type (Subp));

      elsif Is_Access_Type (Etype (Ctrl_Arg)) then
         CW_Typ := Designated_Type (Etype (Ctrl_Arg));

      else
         CW_Typ := Etype (Ctrl_Arg);
      end if;

      Typ := Root_Type (CW_Typ);

      if not Is_Limited_Type (Typ) then
         Eq_Prim_Op := Find_Prim_Op (Typ, Name_Op_Eq);
      end if;

      if Is_CPP_Class (Root_Type (Typ)) then

         --  Create a new parameter list with the displaced 'this'

         New_Params := New_List;
         Param := First_Actual (Call_Node);
         while Present (Param) loop
            Append_To (New_Params, Relocate_Node (Param));
            Next_Actual (Param);
         end loop;

      elsif Present (Param_List) then

         --  Generate the Tag checks when appropriate

         New_Params := New_List;

         Param := First_Actual (Call_Node);
         while Present (Param) loop

            --  No tag check with itself

            if Param = Ctrl_Arg then
               Append_To (New_Params,
                 Duplicate_Subexpr_Move_Checks (Param));

            --  No tag check for parameter whose type is neither tagged nor
            --  access to tagged (for access parameters)

            elsif No (Find_Controlling_Arg (Param)) then
               Append_To (New_Params, Relocate_Node (Param));

            --  No tag check for function dispatching on result if the
            --  Tag given by the context is this one

            elsif Find_Controlling_Arg (Param) = Ctrl_Arg then
               Append_To (New_Params, Relocate_Node (Param));

            --  "=" is the only dispatching operation allowed to get
            --  operands with incompatible tags (it just returns false).
            --  We use Duplicate_Subexpr_Move_Checks instead of calling
            --  Relocate_Node because the value will be duplicated to
            --  check the tags.

            elsif Subp = Eq_Prim_Op then
               Append_To (New_Params,
                 Duplicate_Subexpr_Move_Checks (Param));

            --  No check in presence of suppress flags

            elsif Tag_Checks_Suppressed (Etype (Param))
              or else (Is_Access_Type (Etype (Param))
                         and then Tag_Checks_Suppressed
                                    (Designated_Type (Etype (Param))))
            then
               Append_To (New_Params, Relocate_Node (Param));

            --  Optimization: no tag checks if the parameters are identical

            elsif Is_Entity_Name (Param)
              and then Is_Entity_Name (Ctrl_Arg)
              and then Entity (Param) = Entity (Ctrl_Arg)
            then
               Append_To (New_Params, Relocate_Node (Param));

            --  Now we need to generate the Tag check

            else
               --  Generate code for tag equality check
               --  Perhaps should have Checks.Apply_Tag_Equality_Check???

               Insert_Action (Ctrl_Arg,
                 Make_Implicit_If_Statement (Call_Node,
                   Condition =>
                     Make_Op_Ne (Loc,
                       Left_Opnd =>
                         Make_Selected_Component (Loc,
                           Prefix => New_Value (Ctrl_Arg),
                           Selector_Name =>
                             New_Reference_To
                               (First_Tag_Component (Typ), Loc)),

                       Right_Opnd =>
                         Make_Selected_Component (Loc,
                           Prefix =>
                             Unchecked_Convert_To (Typ, New_Value (Param)),
                           Selector_Name =>
                             New_Reference_To
                               (First_Tag_Component (Typ), Loc))),

                   Then_Statements =>
                     New_List (New_Constraint_Error (Loc))));

               Append_To (New_Params, Relocate_Node (Param));
            end if;

            Next_Actual (Param);
         end loop;
      end if;

      --  Generate the appropriate subprogram pointer type

      if  Etype (Subp) = Typ then
         Res_Typ := CW_Typ;
      else
         Res_Typ := Etype (Subp);
      end if;

      Subp_Typ := Create_Itype (E_Subprogram_Type, Call_Node);
      Subp_Ptr_Typ := Create_Itype (E_Access_Subprogram_Type, Call_Node);
      Set_Etype          (Subp_Typ, Res_Typ);
      Init_Size_Align    (Subp_Ptr_Typ);
      Set_Returns_By_Ref (Subp_Typ, Returns_By_Ref (Subp));

      --  Create a new list of parameters which is a copy of the old formal
      --  list including the creation of a new set of matching entities.

      declare
         Old_Formal : Entity_Id := First_Formal (Subp);
         New_Formal : Entity_Id;
         Extra      : Entity_Id;

      begin
         if Present (Old_Formal) then
            New_Formal := New_Copy (Old_Formal);
            Set_First_Entity (Subp_Typ, New_Formal);
            Param := First_Actual (Call_Node);

            loop
               Set_Scope (New_Formal, Subp_Typ);

               --  Change all the controlling argument types to be class-wide
               --  to avoid a recursion in dispatching.

               if Is_Controlling_Formal (New_Formal) then
                  Set_Etype (New_Formal, Etype (Param));
               end if;

               if Is_Itype (Etype (New_Formal)) then
                  Extra := New_Copy (Etype (New_Formal));

                  if Ekind (Extra) = E_Record_Subtype
                    or else Ekind (Extra) = E_Class_Wide_Subtype
                  then
                     Set_Cloned_Subtype (Extra, Etype (New_Formal));
                  end if;

                  Set_Etype (New_Formal, Extra);
                  Set_Scope (Etype (New_Formal), Subp_Typ);
               end if;

               Extra := New_Formal;
               Next_Formal (Old_Formal);
               exit when No (Old_Formal);

               Set_Next_Entity (New_Formal, New_Copy (Old_Formal));
               Next_Entity (New_Formal);
               Next_Actual (Param);
            end loop;
            Set_Last_Entity (Subp_Typ, Extra);

            --  Copy extra formals

            New_Formal := First_Entity (Subp_Typ);
            while Present (New_Formal) loop
               if Present (Extra_Constrained (New_Formal)) then
                  Set_Extra_Formal (Extra,
                    New_Copy (Extra_Constrained (New_Formal)));
                  Extra := Extra_Formal (Extra);
                  Set_Extra_Constrained (New_Formal, Extra);

               elsif Present (Extra_Accessibility (New_Formal)) then
                  Set_Extra_Formal (Extra,
                    New_Copy (Extra_Accessibility (New_Formal)));
                  Extra := Extra_Formal (Extra);
                  Set_Extra_Accessibility (New_Formal, Extra);
               end if;

               Next_Formal (New_Formal);
            end loop;
         end if;
      end;

      Set_Etype (Subp_Ptr_Typ, Subp_Ptr_Typ);
      Set_Directly_Designated_Type (Subp_Ptr_Typ, Subp_Typ);

      --  If the controlling argument is a value of type Ada.Tag then
      --  use it directly.  Otherwise, the tag must be extracted from
      --  the controlling object.

      if Etype (Ctrl_Arg) = RTE (RE_Tag)
        or else Etype (Ctrl_Arg) = RTE (RE_Interface_Tag)
      then
         Controlling_Tag := Duplicate_Subexpr (Ctrl_Arg);

      else
         Controlling_Tag :=
           Make_Selected_Component (Loc,
             Prefix => Duplicate_Subexpr_Move_Checks (Ctrl_Arg),
             Selector_Name => New_Reference_To (DTC_Entity (Subp), Loc));
      end if;

      --  Generate:
      --   Subp_Ptr_Typ!(Get_Prim_Op_Address (Ctrl._Tag, pos));

      New_Call_Name :=
        Unchecked_Convert_To (Subp_Ptr_Typ,
          Make_DT_Access_Action (Typ,
            Action => Get_Prim_Op_Address,
            Args => New_List (

            --  Vptr

              Controlling_Tag,

            --  Position

              Make_Integer_Literal (Loc, DT_Position (Subp)))));

      if Nkind (Call_Node) = N_Function_Call then

         --  Ada 2005 (AI-251): A dispatching "=" with an abstract interface
         --  just requires the comparison of the tags.

         if Ekind (Etype (Ctrl_Arg)) = E_Class_Wide_Type
           and then Is_Interface (Etype (Ctrl_Arg))
           and then Subp = Eq_Prim_Op
         then
            Param := First_Actual (Call_Node);

            New_Call :=
                Make_Op_Eq (Loc,
                   Left_Opnd =>
                     Make_Selected_Component (Loc,
                       Prefix => New_Value (Param),
                       Selector_Name =>
                         New_Reference_To (First_Tag_Component (Typ), Loc)),

                   Right_Opnd =>
                     Make_Selected_Component (Loc,
                       Prefix =>
                         Unchecked_Convert_To (Typ,
                           New_Value (Next_Actual (Param))),
                       Selector_Name =>
                         New_Reference_To (First_Tag_Component (Typ), Loc)));

         else
            New_Call :=
              Make_Function_Call (Loc,
                Name => New_Call_Name,
                Parameter_Associations => New_Params);

            --  If this is a dispatching "=", we must first compare the tags so
            --  we generate: x.tag = y.tag and then x = y

            if Subp = Eq_Prim_Op then
               Param := First_Actual (Call_Node);
               New_Call :=
                 Make_And_Then (Loc,
                   Left_Opnd =>
                        Make_Op_Eq (Loc,
                          Left_Opnd =>
                            Make_Selected_Component (Loc,
                              Prefix => New_Value (Param),
                              Selector_Name =>
                                New_Reference_To (First_Tag_Component (Typ),
                                                  Loc)),

                          Right_Opnd =>
                            Make_Selected_Component (Loc,
                              Prefix =>
                                Unchecked_Convert_To (Typ,
                                  New_Value (Next_Actual (Param))),
                              Selector_Name =>
                                New_Reference_To (First_Tag_Component (Typ),
                                                  Loc))),
                   Right_Opnd => New_Call);
            end if;
         end if;

      else
         New_Call :=
           Make_Procedure_Call_Statement (Loc,
             Name => New_Call_Name,
             Parameter_Associations => New_Params);
      end if;

      Rewrite (Call_Node, New_Call);
      Analyze_And_Resolve (Call_Node, Call_Typ);
   end Expand_Dispatching_Call;

   ---------------------------------
   -- Expand_Interface_Conversion --
   ---------------------------------

   procedure Expand_Interface_Conversion (N : Node_Id) is
      Loc         : constant Source_Ptr := Sloc (N);
      Operand     : constant Node_Id    := Expression (N);
      Operand_Typ : Entity_Id           := Etype (Operand);
      Target_Type : Entity_Id           := Etype (N);
      Iface_Tag   : Entity_Id;

   begin
      pragma Assert (Nkind (Operand) /= N_Attribute_Reference);

      --  Ada 2005 (AI-345): Set Operand_Typ and Handle task interfaces

      if Ekind (Operand_Typ) = E_Task_Type
        or else Ekind (Operand_Typ) = E_Protected_Type
      then
         Operand_Typ := Corresponding_Record_Type (Operand_Typ);
      end if;

      if Is_Access_Type (Target_Type) then
         Target_Type := Etype (Directly_Designated_Type (Target_Type));

      elsif Is_Class_Wide_Type (Target_Type) then
         Target_Type := Etype (Target_Type);
      end if;

      pragma Assert (not Is_Class_Wide_Type (Target_Type)
        and then Is_Interface (Target_Type));

      Iface_Tag := Find_Interface_Tag (Operand_Typ, Target_Type);

      pragma Assert (Iface_Tag /= Empty);

      Rewrite (N,
        Unchecked_Convert_To (Etype (N),
          Make_Attribute_Reference (Loc,
            Prefix => Make_Selected_Component (Loc,
                        Prefix => Relocate_Node (Expression (N)),
                        Selector_Name => New_Occurrence_Of (Iface_Tag, Loc)),
            Attribute_Name => Name_Address)));

      Analyze (N);
   end Expand_Interface_Conversion;

   ------------------------------
   -- Expand_Interface_Actuals --
   ------------------------------

   procedure Expand_Interface_Actuals (Call_Node : Node_Id) is
      Loc        : constant Source_Ptr := Sloc (Call_Node);
      Actual     : Node_Id;
      Actual_Typ : Entity_Id;
      Conversion : Node_Id;
      Formal     : Entity_Id;
      Formal_Typ : Entity_Id;
      Subp       : Entity_Id;
      Nam        : Name_Id;

   begin
      --  This subprogram is called directly from the semantics, so we need a
      --  check to see whether expansion is active before proceeding.

      if not Expander_Active then
         return;
      end if;

      --  Call using access to subprogram with explicit dereference

      if Nkind (Name (Call_Node)) = N_Explicit_Dereference then
         Subp := Etype (Name (Call_Node));

      --  Normal case

      else
         Subp := Entity (Name (Call_Node));
      end if;

      Formal := First_Formal (Subp);
      Actual := First_Actual (Call_Node);

      while Present (Formal) loop

         pragma Assert (Ekind (Etype (Etype (Formal)))
                        /= E_Record_Type_With_Private);

         --  Ada 2005 (AI-251): Conversion to interface to force "this"
         --  displacement

         Formal_Typ := Etype (Etype (Formal));
         Actual_Typ := Etype (Actual);

         if Is_Interface (Formal_Typ) then

            Conversion := Convert_To (Formal_Typ, New_Copy_Tree (Actual));
            Rewrite             (Actual, Conversion);
            Analyze_And_Resolve (Actual, Formal_Typ);

            Rewrite (Actual,
              Make_Explicit_Dereference (Loc,
                Unchecked_Convert_To
                  (Build_Anonymous_Access_Type (Formal_Typ, Call_Node),
                   Relocate_Node (Expression (Actual)))));

            Analyze_And_Resolve (Actual, Formal_Typ);

         --  Anonymous access type

         elsif Is_Access_Type (Formal_Typ)
           and then Is_Interface (Etype
                                  (Directly_Designated_Type
                                   (Formal_Typ)))
           and then Interface_Present_In_Ancestor
                      (Typ   => Etype (Directly_Designated_Type
                                        (Actual_Typ)),
                       Iface => Etype (Directly_Designated_Type
                                        (Formal_Typ)))
         then

            if Nkind (Actual) = N_Attribute_Reference
              and then
               (Attribute_Name (Actual) = Name_Access
                 or else Attribute_Name (Actual) = Name_Unchecked_Access)
            then
               Nam := Attribute_Name (Actual);

               Conversion :=
                 Convert_To
                   (Etype (Directly_Designated_Type (Formal_Typ)),
                    Prefix (Actual));

               Rewrite (Actual, Conversion);

               Analyze_And_Resolve (Actual,
                 Etype (Directly_Designated_Type (Formal_Typ)));

               Rewrite (Actual,
                 Unchecked_Convert_To (Formal_Typ,
                   Make_Attribute_Reference (Loc,
                     Prefix =>
                       Relocate_Node (Prefix (Expression (Actual))),
                     Attribute_Name => Nam)));

               Analyze_And_Resolve (Actual, Formal_Typ);

            else
               Conversion :=
                 Convert_To (Formal_Typ, New_Copy_Tree (Actual));
               Rewrite             (Actual, Conversion);
               Analyze_And_Resolve (Actual, Formal_Typ);
            end if;
         end if;

         Next_Actual (Actual);
         Next_Formal (Formal);
      end loop;
   end Expand_Interface_Actuals;

   ----------------------------
   -- Expand_Interface_Thunk --
   ----------------------------

   function Expand_Interface_Thunk
     (N           : Node_Id;
      Thunk_Id    : Entity_Id;
      Iface_Tag   : Entity_Id) return Node_Id
   is
      Loc         : constant Source_Ptr := Sloc (N);
      Actuals     : constant List_Id    := New_List;
      Decl        : constant List_Id    := New_List;
      Formals     : constant List_Id    := New_List;
      Thunk_Tag   : constant Node_Id    := Iface_Tag;
      Thunk_Alias : constant Entity_Id  := Alias (Entity (N));
      Target      : Entity_Id;
      New_Code    : Node_Id;
      Formal      : Node_Id;
      New_Formal  : Node_Id;
      Decl_1      : Node_Id;
      Decl_2      : Node_Id;
      Subtyp_Mark : Node_Id;

   begin

      --  Traverse the list of alias to find the final target

      Target := Thunk_Alias;

      while Present (Alias (Target)) loop
         Target := Alias (Target);
      end loop;

      --  Duplicate the formals

      Formal := First_Formal (Thunk_Alias);

      while Present (Formal) loop
         New_Formal := Copy_Separate_Tree (Parent (Formal));

         --  Handle the case in which the subprogram covering
         --  the interface has been inherited:

         --  Example:
         --     type I is interface;
         --     procedure P (X : in I) is abstract;

         --     type T is tagged null record;
         --     procedure P (X : T);

         --     type DT is new T and I with ...

         if Is_Controlling_Formal (Formal) then
            Set_Parameter_Type (New_Formal,
              New_Reference_To (Etype (First_Entity (Entity (N))), Loc));

            --  Why is this line silently commented out ???

            --  New_Reference_To (Etype (Formal), Loc));
         end if;

         Append_To (Formals, New_Formal);
         Next_Formal (Formal);
      end loop;

      if Ekind (First_Formal (Thunk_Alias)) = E_In_Parameter
        and then Ekind (Etype (First_Formal (Thunk_Alias)))
                  = E_Anonymous_Access_Type
      then

         --  Generate:

         --     type T is access all <<type of the first formal>>
         --     S1 := Storage_Offset!(First_formal)
         --           - Storage_Offset!(First_Formal.Thunk_Tag'Position)

         --  ... and the first actual of the call is generated as T!(S1)

         Decl_2 :=
           Make_Full_Type_Declaration (Loc,
             Defining_Identifier =>
               Make_Defining_Identifier (Loc,
                 New_Internal_Name ('T')),
             Type_Definition =>
               Make_Access_To_Object_Definition (Loc,
                 All_Present            => True,
                 Null_Exclusion_Present => False,
                 Constant_Present       => False,
                 Subtype_Indication     =>
                   New_Reference_To
                     (Directly_Designated_Type
                        (Etype (First_Formal (Thunk_Alias))), Loc)
                         ));

         Decl_1 :=
           Make_Object_Declaration (Loc,
             Defining_Identifier =>
               Make_Defining_Identifier (Loc,
                 New_Internal_Name ('S')),
             Constant_Present    => True,
             Object_Definition   =>
               New_Reference_To (RTE (RE_Storage_Offset), Loc),
             Expression          =>
               Make_Op_Subtract (Loc,
                 Left_Opnd  =>
                   Unchecked_Convert_To
                     (RTE (RE_Storage_Offset),
                      New_Reference_To
                        (Defining_Identifier (First (Formals)), Loc)),
                  Right_Opnd =>
                    Unchecked_Convert_To
                      (RTE (RE_Storage_Offset),
                       Make_Attribute_Reference (Loc,
                         Prefix =>
                           Make_Selected_Component (Loc,
                             Prefix =>
                               New_Reference_To
                                 (Defining_Identifier (First (Formals)), Loc),
                             Selector_Name =>
                               New_Occurrence_Of (Thunk_Tag, Loc)),
                         Attribute_Name => Name_Position))));

         Append_To (Decl, Decl_2);
         Append_To (Decl, Decl_1);

         --  Reference the new first actual

         Append_To (Actuals,
           Unchecked_Convert_To
             (Defining_Identifier (Decl_2),
              New_Reference_To (Defining_Identifier (Decl_1), Loc)));

         --  Side note: The reverse order of declarations is just to ensure
         --  that the call to RE_Print is correct.

      else
         --  Generate:
         --
         --     S1 := Storage_Offset!(First_formal'Address)
         --           - Storage_Offset!(First_Formal.Thunk_Tag'Position)
         --     S2 := Tag_Ptr!(S3)

         Decl_1 :=
           Make_Object_Declaration (Loc,
             Defining_Identifier =>
               Make_Defining_Identifier (Loc, New_Internal_Name ('S')),
             Constant_Present    => True,
             Object_Definition   =>
               New_Reference_To (RTE (RE_Storage_Offset), Loc),
             Expression          =>
               Make_Op_Subtract (Loc,
                 Left_Opnd =>
                   Unchecked_Convert_To
                     (RTE (RE_Storage_Offset),
                      Make_Attribute_Reference (Loc,
                        Prefix =>
                          New_Reference_To
                            (Defining_Identifier (First (Formals)), Loc),
                        Attribute_Name => Name_Address)),
                 Right_Opnd =>
                   Unchecked_Convert_To
                     (RTE (RE_Storage_Offset),
                      Make_Attribute_Reference (Loc,
                        Prefix =>
                          Make_Selected_Component (Loc,
                            Prefix =>
                              New_Reference_To
                                (Defining_Identifier (First (Formals)), Loc),
                                 Selector_Name =>
                                   New_Occurrence_Of (Thunk_Tag, Loc)),
                        Attribute_Name => Name_Position))));

         Decl_2 :=
           Make_Object_Declaration (Loc,
             Defining_Identifier =>
               Make_Defining_Identifier (Loc, New_Internal_Name ('S')),
             Constant_Present    => True,
             Object_Definition   => New_Reference_To (RTE (RE_Addr_Ptr), Loc),
             Expression          =>
               Unchecked_Convert_To
                 (RTE (RE_Addr_Ptr),
                  New_Reference_To (Defining_Identifier (Decl_1), Loc)));

         Append_To (Decl, Decl_1);
         Append_To (Decl, Decl_2);

         --  Reference the new first actual

         Append_To (Actuals,
           Unchecked_Convert_To
             (Etype (First_Entity (Target)),
              Make_Explicit_Dereference (Loc,
                New_Reference_To (Defining_Identifier (Decl_2), Loc))));

      end if;

      Formal := Next (First (Formals));
      while Present (Formal) loop
         Append_To (Actuals,
            New_Reference_To (Defining_Identifier (Formal), Loc));
         Next (Formal);
      end loop;

      if Ekind (Thunk_Alias) = E_Procedure then
         New_Code :=
           Make_Subprogram_Body (Loc,
              Specification =>
                Make_Procedure_Specification (Loc,
                  Defining_Unit_Name       => Thunk_Id,
                  Parameter_Specifications => Formals),
              Declarations => Decl,
              Handled_Statement_Sequence =>
                Make_Handled_Sequence_Of_Statements (Loc,
                  Statements => New_List (
                    Make_Procedure_Call_Statement (Loc,
                       Name => New_Occurrence_Of (Target, Loc),
                       Parameter_Associations => Actuals))));

      else pragma Assert (Ekind (Thunk_Alias) = E_Function);

         if not Present (Alias (Thunk_Alias)) then
            Subtyp_Mark := Subtype_Mark (Parent (Thunk_Alias));
         else
            --  The last element in the alias list has the correct subtype_mark
            --  of the function result

            declare
               E : Entity_Id := Alias (Thunk_Alias);
            begin
               while Present (Alias (E)) loop
                  E := Alias (E);
               end loop;
               Subtyp_Mark := Subtype_Mark (Parent (E));
            end;
         end if;

         New_Code :=
           Make_Subprogram_Body (Loc,
              Specification =>
                Make_Function_Specification (Loc,
                  Defining_Unit_Name       => Thunk_Id,
                  Parameter_Specifications => Formals,
                  Subtype_Mark => New_Copy (Subtyp_Mark)),
              Declarations => Decl,
              Handled_Statement_Sequence =>
                Make_Handled_Sequence_Of_Statements (Loc,
                  Statements => New_List (
                    Make_Return_Statement (Loc,
                      Make_Function_Call (Loc,
                        Name => New_Occurrence_Of (Target, Loc),
                        Parameter_Associations => Actuals)))));
      end if;

      Analyze (New_Code);
      Insert_After (N, New_Code);
      return New_Code;
   end Expand_Interface_Thunk;

   -------------
   -- Fill_DT --
   -------------

   function Fill_DT_Entry
     (Loc      : Source_Ptr;
      Prim     : Entity_Id;
      Thunk_Id : Entity_Id := Empty) return Node_Id
   is
      Typ     : constant Entity_Id := Scope (DTC_Entity (Prim));
      DT_Ptr  : Entity_Id := Node (First_Elmt (Access_Disp_Table (Typ)));
      Target  : Entity_Id;
      Tag     : Entity_Id := First_Tag_Component (Typ);
      Prim_Op : Entity_Id := Prim;

   begin
      --  Ada 2005 (AI-251): If we have a thunk available then generate code
      --  that saves its address in the secondary dispatch table of its
      --  abstract interface; otherwise save the address of the primitive
      --  subprogram in the main virtual table.

      if Thunk_Id /= Empty then
         Target := Thunk_Id;
      else
         Target := Prim;
      end if;

      --  Ada 2005 (AI-251): If the subprogram is the alias of an abstract
      --  interface subprogram then find the correct dispatch table pointer

      if Present (Abstract_Interface_Alias (Prim)) then
         Prim_Op := Abstract_Interface_Alias (Prim);

         DT_Ptr  := Find_Interface_ADT
                      (T     => Typ,
                       Iface => Scope (DTC_Entity (Prim_Op)));

         Tag := First_Tag_Component (Scope (DTC_Entity (Prim_Op)));
      end if;

      pragma Assert (DT_Position (Prim_Op) <= DT_Entry_Count (Tag));
      pragma Assert (DT_Position (Prim_Op) > Uint_0);

      return
        Make_DT_Access_Action (Typ,
          Action => Set_Prim_Op_Address,
          Args   => New_List (
            Unchecked_Convert_To (RTE (RE_Tag),
              New_Reference_To (DT_Ptr, Loc)),                  -- DTptr

            Make_Integer_Literal (Loc, DT_Position (Prim_Op)),  -- Position

            Make_Attribute_Reference (Loc,                      -- Value
              Prefix          => New_Reference_To (Target, Loc),
              Attribute_Name  => Name_Address)));
   end Fill_DT_Entry;

   ---------------------------
   -- Get_Remotely_Callable --
   ---------------------------

   function Get_Remotely_Callable (Obj : Node_Id) return Node_Id is
      Loc : constant Source_Ptr := Sloc (Obj);

   begin
      return Make_DT_Access_Action
        (Typ    => Etype (Obj),
         Action => Get_Remotely_Callable,
         Args   => New_List (
           Make_Selected_Component (Loc,
             Prefix        => Obj,
             Selector_Name => Make_Identifier (Loc, Name_uTag))));
   end Get_Remotely_Callable;

   -------------
   -- Make_DT --
   -------------

   function Make_DT (Typ : Entity_Id) return List_Id is
      Loc         : constant Source_Ptr := Sloc (Typ);
      Result      : constant List_Id    := New_List;
      Elab_Code   : constant List_Id    := New_List;

      Tname       : constant Name_Id := Chars (Typ);
      Name_DT     : constant Name_Id := New_External_Name (Tname, 'T');
      Name_DT_Ptr : constant Name_Id := New_External_Name (Tname, 'P');
      Name_TSD    : constant Name_Id := New_External_Name (Tname, 'B');
      Name_Exname : constant Name_Id := New_External_Name (Tname, 'E');
      Name_No_Reg : constant Name_Id := New_External_Name (Tname, 'F');

      DT     : constant Node_Id := Make_Defining_Identifier (Loc, Name_DT);
      DT_Ptr : constant Node_Id := Make_Defining_Identifier (Loc, Name_DT_Ptr);
      TSD    : constant Node_Id := Make_Defining_Identifier (Loc, Name_TSD);
      Exname : constant Node_Id := Make_Defining_Identifier (Loc, Name_Exname);
      No_Reg : constant Node_Id := Make_Defining_Identifier (Loc, Name_No_Reg);

      Generalized_Tag : constant Entity_Id := RTE (RE_Tag);
      I_Depth         : Int;
      Size_Expr_Node  : Node_Id;
      Old_Tag1        : Node_Id;
      Old_Tag2        : Node_Id;
      Num_Ifaces      : Int;
      Nb_Prim         : Int;
      TSD_Num_Entries : Int;
      Typ_Copy        : constant Entity_Id := New_Copy (Typ);
      AI              : Elmt_Id;

   begin
      if not RTE_Available (RE_Tag) then
         Error_Msg_CRT ("tagged types", Typ);
         return New_List;
      end if;

      --  Collect the full list of directly and indirectly implemented
      --  interfaces

      Set_Parent              (Typ_Copy, Parent (Typ));
      Set_Abstract_Interfaces (Typ_Copy, New_Elmt_List);
      Collect_All_Interfaces  (Typ_Copy);

      --  Calculate the number of entries required in the table of interfaces

      Num_Ifaces := 0;
      AI         := First_Elmt (Abstract_Interfaces (Typ_Copy));

      while Present (AI) loop
         Num_Ifaces := Num_Ifaces + 1;
         Next_Elmt (AI);
      end loop;

      --  Count ancestors to compute the inheritance depth. For private
      --  extensions, always go to the full view in order to compute the real
      --  inheritance depth.

      declare
         Parent_Type : Entity_Id := Typ;
         P           : Entity_Id;

      begin
         I_Depth := 0;

         loop
            P := Etype (Parent_Type);

            if Is_Private_Type (P) then
               P := Full_View (Base_Type (P));
            end if;

            exit when P = Parent_Type;

            I_Depth := I_Depth + 1;
            Parent_Type := P;
         end loop;
      end;

      TSD_Num_Entries := I_Depth + Num_Ifaces + 1;
      Nb_Prim := UI_To_Int (DT_Entry_Count (First_Tag_Component (Typ)));

      --  ----------------------------------------------------------------

      --  Dispatch table and related entities are allocated statically

      Set_Ekind (DT, E_Variable);
      Set_Is_Statically_Allocated (DT);

      Set_Ekind (DT_Ptr, E_Variable);
      Set_Is_Statically_Allocated (DT_Ptr);

      Set_Ekind (TSD, E_Variable);
      Set_Is_Statically_Allocated (TSD);

      Set_Ekind (Exname, E_Variable);
      Set_Is_Statically_Allocated (Exname);

      Set_Ekind (No_Reg, E_Variable);
      Set_Is_Statically_Allocated (No_Reg);

      --  Generate code to create the storage for the Dispatch_Table object:

      --   DT : Storage_Array (1..DT_Prologue_Size+nb_prim*DT_Entry_Size);
      --   for DT'Alignment use Address'Alignment

      Size_Expr_Node :=
        Make_Op_Add (Loc,
          Left_Opnd  => Make_DT_Access_Action (Typ, DT_Prologue_Size, No_List),
          Right_Opnd =>
            Make_Op_Multiply (Loc,
              Left_Opnd  =>
                Make_DT_Access_Action (Typ, DT_Entry_Size, No_List),
              Right_Opnd =>
                Make_Integer_Literal (Loc, Nb_Prim)));

      Append_To (Result,
        Make_Object_Declaration (Loc,
          Defining_Identifier => DT,
          Aliased_Present     => True,
          Object_Definition   =>
            Make_Subtype_Indication (Loc,
              Subtype_Mark => New_Reference_To (RTE (RE_Storage_Array), Loc),
              Constraint   => Make_Index_Or_Discriminant_Constraint (Loc,
                Constraints => New_List (
                  Make_Range (Loc,
                    Low_Bound  => Make_Integer_Literal (Loc, 1),
                    High_Bound => Size_Expr_Node))))));

      Append_To (Result,
        Make_Attribute_Definition_Clause (Loc,
          Name       => New_Reference_To (DT, Loc),
          Chars      => Name_Alignment,
          Expression =>
            Make_Attribute_Reference (Loc,
              Prefix => New_Reference_To (RTE (RE_Integer_Address), Loc),
              Attribute_Name => Name_Alignment)));

      --  Generate code to create the pointer to the dispatch table

      --    DT_Ptr : Tag := Tag!(DT'Address);

      --  According to the C++ ABI, the base of the vtable is located after a
      --  prologue containing Offset_To_Top, and Typeinfo_Ptr. Hence, we move
      --  down the pointer to the real base of the vtable

      Append_To (Result,
        Make_Object_Declaration (Loc,
          Defining_Identifier => DT_Ptr,
          Constant_Present    => True,
          Object_Definition   => New_Reference_To (Generalized_Tag, Loc),
          Expression          =>
            Unchecked_Convert_To (Generalized_Tag,
              Make_Op_Add (Loc,
                Left_Opnd =>
                  Unchecked_Convert_To (RTE (RE_Storage_Offset),
                    Make_Attribute_Reference (Loc,
                      Prefix         => New_Reference_To (DT, Loc),
                      Attribute_Name => Name_Address)),
                Right_Opnd =>
                  Make_DT_Access_Action (Typ,
                    DT_Prologue_Size, No_List)))));

      --  Generate code to define the boolean that controls registration, in
      --  order to avoid multiple registrations for tagged types defined in
      --  multiple-called scopes

      Append_To (Result,
        Make_Object_Declaration (Loc,
          Defining_Identifier => No_Reg,
          Object_Definition   => New_Reference_To (Standard_Boolean, Loc),
          Expression          => New_Reference_To (Standard_True, Loc)));

      --  Set Access_Disp_Table field to be the dispatch table pointer

      if not Present (Access_Disp_Table (Typ)) then
         Set_Access_Disp_Table (Typ, New_Elmt_List);
      end if;

      Prepend_Elmt (DT_Ptr, Access_Disp_Table (Typ));

      --  Generate code to create the storage for the type specific data object
      --  with enough space to store the tags of the ancestors plus the tags
      --  of all the implemented interfaces (as described in a-tags.adb)
      --
      --   TSD: Storage_Array
      --     (1..TSD_Prologue_Size+TSD_Num_Entries*TSD_Entry_Size);
      --   for TSD'Alignment use Address'Alignment

      Size_Expr_Node :=
        Make_Op_Add (Loc,
          Left_Opnd  =>
            Make_DT_Access_Action (Typ, TSD_Prologue_Size, No_List),
          Right_Opnd =>
            Make_Op_Multiply (Loc,
              Left_Opnd  =>
                Make_DT_Access_Action (Typ, TSD_Entry_Size, No_List),
              Right_Opnd =>
                Make_Integer_Literal (Loc, TSD_Num_Entries)));

      Append_To (Result,
        Make_Object_Declaration (Loc,
          Defining_Identifier => TSD,
          Aliased_Present     => True,
          Object_Definition   =>
            Make_Subtype_Indication (Loc,
              Subtype_Mark => New_Reference_To (RTE (RE_Storage_Array), Loc),
              Constraint   => Make_Index_Or_Discriminant_Constraint (Loc,
                Constraints => New_List (
                  Make_Range (Loc,
                    Low_Bound  => Make_Integer_Literal (Loc, 1),
                    High_Bound => Size_Expr_Node))))));

      Append_To (Result,
        Make_Attribute_Definition_Clause (Loc,
          Name       => New_Reference_To (TSD, Loc),
          Chars      => Name_Alignment,
          Expression =>
            Make_Attribute_Reference (Loc,
              Prefix => New_Reference_To (RTE (RE_Integer_Address), Loc),
              Attribute_Name => Name_Alignment)));

      --  Generate code to put the Address of the TSD in the dispatch table
      --    Set_TSD (DT_Ptr, TSD);

      Append_To (Elab_Code,
        Make_DT_Access_Action (Typ,
          Action => Set_TSD,
          Args   => New_List (
            New_Reference_To (DT_Ptr, Loc),                  -- DTptr
              Make_Attribute_Reference (Loc,                 -- Value
              Prefix          => New_Reference_To (TSD, Loc),
              Attribute_Name  => Name_Address))));

      --  Generate: Exname : constant String := full_qualified_name (typ);
      --  The type itself may be an anonymous parent type, so use the first
      --  subtype to have a user-recognizable name.

      Append_To (Result,
        Make_Object_Declaration (Loc,
          Defining_Identifier => Exname,
          Constant_Present    => True,
          Object_Definition   => New_Reference_To (Standard_String, Loc),
          Expression =>
            Make_String_Literal (Loc,
              Full_Qualified_Name (First_Subtype (Typ)))));

      --  Generate: Set_Expanded_Name (DT_Ptr, exname'Address);

      Append_To (Elab_Code,
        Make_DT_Access_Action (Typ,
          Action => Set_Expanded_Name,
          Args   => New_List (
            Node1 => New_Reference_To (DT_Ptr, Loc),
            Node2 =>
              Make_Attribute_Reference (Loc,
                Prefix => New_Reference_To (Exname, Loc),
                Attribute_Name => Name_Address))));

      --  Generate: Set_Access_Level (DT_Ptr, <type's accessibility level>);

      Append_To (Elab_Code,
        Make_DT_Access_Action (Typ,
          Action => Set_Access_Level,
          Args   => New_List (
            Node1 => New_Reference_To (DT_Ptr, Loc),
            Node2 => Make_Integer_Literal (Loc, Type_Access_Level (Typ)))));

      --  Generate:
      --    Set_Offset_To_Top (DT_Ptr, 0);

      Append_To (Elab_Code,
        Make_Procedure_Call_Statement (Loc,
          Name => New_Reference_To (RTE (RE_Set_Offset_To_Top), Loc),
          Parameter_Associations => New_List (
            New_Reference_To (DT_Ptr, Loc),
            Make_Integer_Literal (Loc, Uint_0))));

      if Typ = Etype (Typ)
        or else Is_CPP_Class (Etype (Typ))
      then
         Old_Tag1 :=
           Unchecked_Convert_To (Generalized_Tag,
             Make_Integer_Literal (Loc, 0));
         Old_Tag2 :=
           Unchecked_Convert_To (Generalized_Tag,
             Make_Integer_Literal (Loc, 0));

      else
         Old_Tag1 :=
           New_Reference_To
             (Node (First_Elmt (Access_Disp_Table (Etype (Typ)))), Loc);
         Old_Tag2 :=
           New_Reference_To
             (Node (First_Elmt (Access_Disp_Table (Etype (Typ)))), Loc);
      end if;

      --  Generate: Inherit_DT (parent'tag, DT_Ptr, nb_prim of parent);

      Append_To (Elab_Code,
        Make_DT_Access_Action (Typ,
          Action => Inherit_DT,
          Args   => New_List (
            Node1 => Old_Tag1,
            Node2 => New_Reference_To (DT_Ptr, Loc),
            Node3 => Make_Integer_Literal (Loc,
                       DT_Entry_Count (First_Tag_Component (Etype (Typ)))))));

      --  Generate: Inherit_TSD (parent'tag, DT_Ptr);

      Append_To (Elab_Code,
        Make_DT_Access_Action (Typ,
          Action => Inherit_TSD,
          Args   => New_List (
            Node1 => Old_Tag2,
            Node2 => New_Reference_To (DT_Ptr, Loc))));

      --  for types with no controlled components
      --    Generate: Set_RC_Offset (DT_Ptr, 0);
      --  for simple types with controlled components
      --    Generate: Set_RC_Offset (DT_Ptr, type._record_controller'position);
      --  for complex types with controlled components where the position
      --  of the record controller is not statically computable, if there are
      --  controlled components at this level
      --    Generate: Set_RC_Offset (DT_Ptr, -1);
      --  to indicate that the _controller field is right after the _parent or
      --  if there are no controlled components at this level,
      --    Generate: Set_RC_Offset (DT_Ptr, -2);
      --  to indicate that we need to get the position from the parent.

      declare
         Position : Node_Id;

      begin
         if not Has_Controlled_Component (Typ) then
            Position := Make_Integer_Literal (Loc, 0);

         elsif Etype (Typ) /= Typ and then Has_Discriminants (Etype (Typ)) then
            if Has_New_Controlled_Component (Typ) then
               Position := Make_Integer_Literal (Loc, -1);
            else
               Position := Make_Integer_Literal (Loc, -2);
            end if;
         else
            Position :=
              Make_Attribute_Reference (Loc,
                Prefix =>
                  Make_Selected_Component (Loc,
                    Prefix => New_Reference_To (Typ, Loc),
                    Selector_Name =>
                      New_Reference_To (Controller_Component (Typ), Loc)),
                Attribute_Name => Name_Position);

            --  This is not proper Ada code to use the attribute 'Position
            --  on something else than an object but this is supported by
            --  the back end (see comment on the Bit_Component attribute in
            --  sem_attr). So we avoid semantic checking here.

            Set_Analyzed (Position);
            Set_Etype (Prefix (Position), RTE (RE_Record_Controller));
            Set_Etype (Prefix (Prefix (Position)), Typ);
            Set_Etype (Selector_Name (Prefix (Position)),
              RTE (RE_Record_Controller));
            Set_Etype (Position, RTE (RE_Storage_Offset));
         end if;

         Append_To (Elab_Code,
           Make_DT_Access_Action (Typ,
             Action => Set_RC_Offset,
             Args   => New_List (
               Node1 => New_Reference_To (DT_Ptr, Loc),
               Node2 => Position)));
      end;

      --  Generate: Set_Remotely_Callable (DT_Ptr, Status);
      --  where Status is described in E.4 (18)

      declare
         Status : Entity_Id;

      begin
         Status :=
           Boolean_Literals
             (Is_Pure (Typ)
                or else Is_Shared_Passive (Typ)
                or else
                  ((Is_Remote_Types (Typ)
                      or else Is_Remote_Call_Interface (Typ))
                   and then Original_View_In_Visible_Part (Typ))
                or else not Comes_From_Source (Typ));

         Append_To (Elab_Code,
           Make_DT_Access_Action (Typ,
             Action => Set_Remotely_Callable,
             Args   => New_List (
               New_Occurrence_Of (DT_Ptr, Loc),
               New_Occurrence_Of (Status, Loc))));
      end;

      --  Generate: Set_External_Tag (DT_Ptr, exname'Address);
      --  Should be the external name not the qualified name???

      if not Has_External_Tag_Rep_Clause (Typ) then
         Append_To (Elab_Code,
           Make_DT_Access_Action (Typ,
             Action => Set_External_Tag,
             Args   => New_List (
               Node1 => New_Reference_To (DT_Ptr, Loc),
               Node2 =>
                 Make_Attribute_Reference (Loc,
                   Prefix => New_Reference_To (Exname, Loc),
                   Attribute_Name => Name_Address))));

      --  Generate code to register the Tag in the External_Tag hash
      --  table for the pure Ada type only.

      --        Register_Tag (Dt_Ptr);

      --  Skip this if routine not available, or in No_Run_Time mode

         if RTE_Available (RE_Register_Tag)
           and then Is_RTE (Generalized_Tag, RE_Tag)
           and then not No_Run_Time_Mode
         then
            Append_To (Elab_Code,
              Make_Procedure_Call_Statement (Loc,
                Name => New_Reference_To (RTE (RE_Register_Tag), Loc),
                Parameter_Associations =>
                  New_List (New_Reference_To (DT_Ptr, Loc))));
         end if;
      end if;

      --  Generate:
      --     if No_Reg then
      --        <elab_code>
      --        No_Reg := False;
      --     end if;

      Append_To (Elab_Code,
        Make_Assignment_Statement (Loc,
          Name       => New_Reference_To (No_Reg, Loc),
          Expression => New_Reference_To (Standard_False, Loc)));

      Append_To (Result,
        Make_Implicit_If_Statement (Typ,
          Condition       => New_Reference_To (No_Reg, Loc),
          Then_Statements => Elab_Code));

      --  Ada 2005 (AI-251): Register the tag of the interfaces into
      --  the table of implemented interfaces

      if Present (Abstract_Interfaces (Typ))
        and then not Is_Empty_Elmt_List (Abstract_Interfaces (Typ))
      then
         AI := First_Elmt (Abstract_Interfaces (Typ_Copy));
         while Present (AI) loop

            --  Generate:
            --    Register_Interface (DT_Ptr, Interface'Tag);

            Append_To (Result,
              Make_DT_Access_Action (Typ,
                Action => Register_Interface_Tag,
                Args   => New_List (
                  Node1 => New_Reference_To (DT_Ptr, Loc),
                  Node2 => New_Reference_To
                             (Node
                              (First_Elmt
                               (Access_Disp_Table (Node (AI)))),
                              Loc))));

            Next_Elmt (AI);
         end loop;
      end if;

      return Result;
   end Make_DT;

   --------------------------------
   -- Make_Abstract_Interface_DT --
   --------------------------------

   procedure Make_Abstract_Interface_DT
     (AI_Tag          : Entity_Id;
      Acc_Disp_Tables : in out Elist_Id;
      Result          : out List_Id)
   is
      Loc         : constant Source_Ptr := Sloc (AI_Tag);
      Tname       : constant Name_Id := Chars (AI_Tag);
      Name_DT     : constant Name_Id := New_External_Name (Tname, 'T');
      Name_DT_Ptr : constant Name_Id := New_External_Name (Tname, 'P');

      Iface_DT     : constant Node_Id :=
                       Make_Defining_Identifier (Loc, Name_DT);
      Iface_DT_Ptr : constant Node_Id :=
                       Make_Defining_Identifier (Loc, Name_DT_Ptr);

      Generalized_Tag : constant Entity_Id := RTE (RE_Interface_Tag);
      Size_Expr_Node  : Node_Id;
      Nb_Prim         : Int;

   begin
      Result := New_List;

      --  Dispatch table and related entities are allocated statically

      Set_Ekind (Iface_DT, E_Variable);
      Set_Is_Statically_Allocated (Iface_DT);

      Set_Ekind (Iface_DT_Ptr, E_Variable);
      Set_Is_Statically_Allocated (Iface_DT_Ptr);

      --  Generate code to create the storage for the Dispatch_Table object

      --    DT : Storage_Array (1..DT_Prologue_Size+nb_prim*DT_Entry_Size);
      --    for DT'Alignment use Address'Alignment

      Nb_Prim := UI_To_Int (DT_Entry_Count (AI_Tag));

      Size_Expr_Node :=
        Make_Op_Add (Loc,
          Left_Opnd  => Make_DT_Access_Action (Etype (AI_Tag),
                          DT_Prologue_Size,
                          No_List),
          Right_Opnd =>
            Make_Op_Multiply (Loc,
              Left_Opnd  =>
                Make_DT_Access_Action (Etype (AI_Tag),
                                       DT_Entry_Size,
                                       No_List),
              Right_Opnd =>
                Make_Integer_Literal (Loc, Nb_Prim)));

      Append_To (Result,
        Make_Object_Declaration (Loc,
          Defining_Identifier => Iface_DT,
          Aliased_Present     => True,
          Object_Definition   =>
            Make_Subtype_Indication (Loc,
              Subtype_Mark => New_Reference_To (RTE (RE_Storage_Array), Loc),
              Constraint   => Make_Index_Or_Discriminant_Constraint (Loc,
                Constraints => New_List (
                  Make_Range (Loc,
                    Low_Bound  => Make_Integer_Literal (Loc, 1),
                    High_Bound => Size_Expr_Node)))),

            --  Initialize the signature of the interface tag. It is currently
            --  a sequence of four bytes located in the unused Typeinfo_Ptr
            --  field of the prologue). Its current value is the following
            --  sequence: (80, Nb_Prim, 0, 80)

          Expression =>
            Make_Aggregate (Loc,
              Component_Associations => New_List (
                Make_Component_Association (Loc,

                  --  -80, 0, 0, -80

                  Choices => New_List (
                    Make_Integer_Literal (Loc, Uint_5),
                    Make_Integer_Literal (Loc, Uint_8)),
                  Expression =>
                    Make_Integer_Literal (Loc, Uint_80)),

                Make_Component_Association (Loc,
                  Choices => New_List (
                    Make_Integer_Literal (Loc, Uint_2)),
                  Expression =>
                    Make_Integer_Literal (Loc, Nb_Prim)),

                Make_Component_Association (Loc,
                  Choices => New_List (
                    Make_Others_Choice (Loc)),
                  Expression => Make_Integer_Literal (Loc, Uint_0))))));

      Append_To (Result,
        Make_Attribute_Definition_Clause (Loc,
          Name       => New_Reference_To (Iface_DT, Loc),
          Chars      => Name_Alignment,
          Expression =>
            Make_Attribute_Reference (Loc,
              Prefix => New_Reference_To (RTE (RE_Integer_Address), Loc),
              Attribute_Name => Name_Alignment)));

      --  Generate code to create the pointer to the dispatch table

      --    Iface_DT_Ptr : Tag := Tag!(DT'Address);

      --  According to the C++ ABI, the base of the vtable is located
      --  after the following prologue: Offset_To_Top, and Typeinfo_Ptr.
      --  Hence, move the pointer down to the real base of the vtable.

      Append_To (Result,
        Make_Object_Declaration (Loc,
          Defining_Identifier => Iface_DT_Ptr,
          Constant_Present    => True,
          Object_Definition   => New_Reference_To (Generalized_Tag, Loc),
          Expression          =>
            Unchecked_Convert_To (Generalized_Tag,
              Make_Op_Add (Loc,
                Left_Opnd =>
                  Unchecked_Convert_To (RTE (RE_Storage_Offset),
                    Make_Attribute_Reference (Loc,
                      Prefix         => New_Reference_To (Iface_DT, Loc),
                      Attribute_Name => Name_Address)),
                Right_Opnd =>
                  Make_DT_Access_Action (Etype (AI_Tag),
                    DT_Prologue_Size, No_List)))));

      --  Note: Offset_To_Top will be initialized by the init subprogram

      --  Set Access_Disp_Table field to be the dispatch table pointer

      if not (Present (Acc_Disp_Tables)) then
         Acc_Disp_Tables := New_Elmt_List;
      end if;

      Append_Elmt (Iface_DT_Ptr, Acc_Disp_Tables);

   end Make_Abstract_Interface_DT;

   ---------------------------
   -- Make_DT_Access_Action --
   ---------------------------

   function Make_DT_Access_Action
     (Typ    : Entity_Id;
      Action : DT_Access_Action;
      Args   : List_Id) return Node_Id
   is
      Action_Name : constant Entity_Id := RTE (Ada_Actions (Action));
      Loc         : Source_Ptr;

   begin
      if No (Args) then

         --  This is a constant

         return New_Reference_To (Action_Name, Sloc (Typ));
      end if;

      pragma Assert (List_Length (Args) = Action_Nb_Arg (Action));

      Loc := Sloc (First (Args));

      if Action_Is_Proc (Action) then
         return
           Make_Procedure_Call_Statement (Loc,
             Name => New_Reference_To (Action_Name, Loc),
             Parameter_Associations => Args);

      else
         return
           Make_Function_Call (Loc,
             Name => New_Reference_To (Action_Name, Loc),
             Parameter_Associations => Args);
      end if;
   end Make_DT_Access_Action;

   -----------------------------------
   -- Original_View_In_Visible_Part --
   -----------------------------------

   function Original_View_In_Visible_Part (Typ : Entity_Id) return Boolean is
      Scop : constant Entity_Id := Scope (Typ);

   begin
      --  The scope must be a package

      if Ekind (Scop) /= E_Package
        and then Ekind (Scop) /= E_Generic_Package
      then
         return False;
      end if;

      --  A type with a private declaration has a private view declared in
      --  the visible part.

      if Has_Private_Declaration (Typ) then
         return True;
      end if;

      return List_Containing (Parent (Typ)) =
        Visible_Declarations (Specification (Unit_Declaration_Node (Scop)));
   end Original_View_In_Visible_Part;

   -------------------------
   -- Set_All_DT_Position --
   -------------------------

   procedure Set_All_DT_Position (Typ : Entity_Id) is
      Parent_Typ : constant Entity_Id := Etype (Typ);
      Root_Typ   : constant Entity_Id := Root_Type (Typ);
      First_Prim : constant Elmt_Id := First_Elmt (Primitive_Operations (Typ));
      The_Tag    : constant Entity_Id := First_Tag_Component (Typ);

      Adjusted   : Boolean := False;
      Finalized  : Boolean := False;

      Count_Prim : Int;
      DT_Length  : Int;
      Nb_Prim    : Int;
      Parent_EC  : Int;
      Prim       : Entity_Id;
      Prim_Elmt  : Elmt_Id;

      procedure Validate_Position (Prim : Entity_Id);
      --  Check that the position assignated to Prim is completely safe
      --  (it has not been assigned to a previously defined primitive
      --   operation of Typ)

      -----------------------
      -- Validate_Position --
      -----------------------

      procedure Validate_Position (Prim : Entity_Id) is
         Prim_Elmt : Elmt_Id;
      begin
         Prim_Elmt :=  First_Elmt (Primitive_Operations (Typ));
         while Present (Prim_Elmt)
            and then Node (Prim_Elmt) /= Prim
         loop
            --  Primitive operations covering abstract interfaces are
            --  allocated later

            if Present (Abstract_Interface_Alias (Node (Prim_Elmt))) then
               null;

            --  Predefined dispatching operations are completely safe.
            --  They are allocated at fixed positions.

            elsif Is_Predefined_Dispatching_Operation (Node (Prim_Elmt)) then
               null;

            --  Aliased subprograms are safe

            elsif Present (Alias (Prim)) then
               null;

            elsif DT_Position (Node (Prim_Elmt)) = DT_Position (Prim) then
               raise Program_Error;
            end if;

            Next_Elmt (Prim_Elmt);
         end loop;
      end Validate_Position;

   --  Start of processing for Set_All_DT_Position

   begin
      --  Get Entry_Count of the parent

      if Parent_Typ /= Typ
        and then DT_Entry_Count (First_Tag_Component (Parent_Typ)) /= No_Uint
      then
         Parent_EC := UI_To_Int (DT_Entry_Count
                                   (First_Tag_Component (Parent_Typ)));
      else
         Parent_EC := 0;
      end if;

      --  C++ Case, check that pragma CPP_Class, CPP_Virtual and CPP_Vtable
      --  give a coherent set of information

      if Is_CPP_Class (Root_Typ) then

         --  Compute the number of primitive operations in the main Vtable
         --  Set their position:
         --    - where it was set if overriden or inherited
         --    - after the end of the parent vtable otherwise

         Prim_Elmt := First_Prim;
         Nb_Prim := 0;
         while Present (Prim_Elmt) loop
            Prim := Node (Prim_Elmt);

            if not Is_CPP_Class (Typ) then
               Set_DTC_Entity (Prim, The_Tag);

            elsif Present (Alias (Prim)) then
               Set_DTC_Entity (Prim, DTC_Entity (Alias (Prim)));
               Set_DT_Position (Prim, DT_Position (Alias (Prim)));

            elsif No (DTC_Entity (Prim)) and then Is_CPP_Class (Typ) then
                  Error_Msg_NE ("is a primitive operation of&," &
                    " pragma Cpp_Virtual required", Prim, Typ);
            end if;

            if DTC_Entity (Prim) = The_Tag then

               --  Get the slot from the parent subprogram if any

               declare
                  H : Entity_Id := Homonym (Prim);

               begin
                  while Present (H) loop
                     if Present (DTC_Entity (H))
                       and then Root_Type (Scope (DTC_Entity (H))) = Root_Typ
                     then
                        Set_DT_Position (Prim, DT_Position (H));
                        exit;
                     end if;

                     H := Homonym (H);
                  end loop;
               end;

               --  Otherwise take the canonical slot after the end of the
               --  parent Vtable

               if DT_Position (Prim) = No_Uint then
                  Nb_Prim := Nb_Prim + 1;
                  Set_DT_Position (Prim, UI_From_Int (Parent_EC + Nb_Prim));

               elsif UI_To_Int (DT_Position (Prim)) > Parent_EC then
                  Nb_Prim := Nb_Prim + 1;
               end if;
            end if;

            Next_Elmt (Prim_Elmt);
         end loop;

         --  Check that the declared size of the Vtable is bigger or equal
         --  than the number of primitive operations (if bigger it means that
         --  some of the c++ virtual functions were not imported, that is
         --  allowed)

         if DT_Entry_Count (The_Tag) = No_Uint
           or else not Is_CPP_Class (Typ)
         then
            Set_DT_Entry_Count (The_Tag, UI_From_Int (Parent_EC + Nb_Prim));

         elsif UI_To_Int (DT_Entry_Count (The_Tag)) < Parent_EC + Nb_Prim then
            Error_Msg_N ("not enough room in the Vtable for all virtual"
              & " functions", The_Tag);
         end if;

         --  Check that Positions are not duplicate nor outside the range of
         --  the Vtable

         declare
            Size : constant Int := UI_To_Int (DT_Entry_Count (The_Tag));
            Pos  : Int;
            Prim_Pos_Table : array (1 .. Size) of Entity_Id :=
                                                        (others => Empty);

         begin
            Prim_Elmt := First_Prim;
            while Present (Prim_Elmt) loop
               Prim := Node (Prim_Elmt);

               if DTC_Entity (Prim) = The_Tag then
                  Pos := UI_To_Int (DT_Position (Prim));

                  if Pos not in Prim_Pos_Table'Range then
                     Error_Msg_N
                       ("position not in range of virtual table", Prim);

                  elsif Present (Prim_Pos_Table (Pos)) then
                     Error_Msg_NE ("cannot be at the same position in the"
                       & " vtable than&", Prim, Prim_Pos_Table (Pos));

                  else
                     Prim_Pos_Table (Pos) := Prim;
                  end if;
               end if;

               Next_Elmt (Prim_Elmt);
            end loop;
         end;

      --  For regular Ada tagged types, just set the DT_Position for
      --  each primitive operation. Perform some sanity checks to avoid
      --  to build completely inconsistant dispatch tables.

      --  Note that the _Size primitive is always set at position 1 in order
      --  to comply with the needs of Ada.Tags.Parent_Size (see documentation
      --  in a-tags.ad?)

      else
         --  First stage: Set the DTC entity of all the primitive operations
         --  This is required to properly read the DT_Position attribute in
         --  the latter stages.

         Prim_Elmt  := First_Prim;
         Count_Prim := 0;
         while Present (Prim_Elmt) loop
            Count_Prim := Count_Prim + 1;
            Prim       := Node (Prim_Elmt);

            --  Ada 2005 (AI-251)

            if Present (Abstract_Interface_Alias (Prim)) then
               Set_DTC_Entity (Prim,
                  Find_Interface_Tag
                    (T => Typ,
                     Iface => Scope (DTC_Entity
                                      (Abstract_Interface_Alias (Prim)))));

            else
               Set_DTC_Entity (Prim, The_Tag);
            end if;

            --  Clear any previous value of the DT_Position attribute. In this
            --  way we ensure that the final position of all the primitives is
            --  stablished by the following stages of this algorithm.

            Set_DT_Position (Prim, No_Uint);

            Next_Elmt (Prim_Elmt);
         end loop;

         declare
            Fixed_Prim : array (Int range 0 .. 10 + Parent_EC + Count_Prim)
                            of Boolean := (others => False);
            E          : Entity_Id;

         begin
            --  Second stage: Register fixed entries

            Nb_Prim   := 10;
            Prim_Elmt := First_Prim;

            while Present (Prim_Elmt) loop
               Prim := Node (Prim_Elmt);

               --  Predefined primitives have a fixed position in all the
               --  dispatch tables

               if Is_Predefined_Dispatching_Operation (Prim) then
                  Set_DT_Position (Prim, Default_Prim_Op_Position (Prim));
                  Fixed_Prim (UI_To_Int (DT_Position (Prim))) := True;

               --  Overriding interface primitives of an ancestor

               elsif DT_Position (Prim) = No_Uint
                 and then Present (Abstract_Interface_Alias (Prim))
                 and then Present (DTC_Entity
                                   (Abstract_Interface_Alias (Prim)))
                 and then DT_Position (Abstract_Interface_Alias (Prim))
                                        /= No_Uint
                 and then Is_Inherited_Operation (Prim)
                 and then Is_Ancestor (Scope
                                       (DTC_Entity
                                        (Abstract_Interface_Alias (Prim))),
                                       Typ)
               then
                  Set_DT_Position (Prim,
                    DT_Position (Abstract_Interface_Alias (Prim)));
                  Set_DT_Position (Alias (Prim),
                    DT_Position (Abstract_Interface_Alias (Prim)));
                  Fixed_Prim (UI_To_Int (DT_Position (Prim))) := True;

               --  Overriding primitives must use the same entry as the
               --  overriden primitive

               elsif DT_Position (Prim) = No_Uint
                 and then Present (Alias (Prim))
                 and then Present (DTC_Entity (Alias (Prim)))
                 and then DT_Position (Alias (Prim)) /= No_Uint
                 and then Is_Inherited_Operation (Prim)
                 and then Is_Ancestor (Scope (DTC_Entity (Alias (Prim))), Typ)
               then
                  E := Alias (Prim);
                  while not (Present (DTC_Entity (E))
                              or else DT_Position (E) = No_Uint)
                    and then Present (Alias (E))
                  loop
                     E := Alias (E);
                  end loop;

                  pragma Assert (Present (DTC_Entity (E))
                                   and then
                                 DT_Position (E) /= No_Uint);

                  Set_DT_Position (Prim, DT_Position (E));
                  Fixed_Prim (UI_To_Int (DT_Position (E))) := True;

                  --  If this is not the last element in the chain continue
                  --  traversing the chain. This is required to properly
                  --  handling renamed primitives

                  if Present (Alias (E)) then
                     while Present (Alias (E)) loop
                        E   := Alias (E);
                        Fixed_Prim (UI_To_Int (DT_Position (E))) := True;
                     end loop;
                  end if;
               end if;

               Next_Elmt (Prim_Elmt);
            end loop;

            --  Third stage: Fix the position of all the new primitives
            --  Entries associated with primitives covering interfaces
            --  are handled in a latter round.

            Prim_Elmt := First_Prim;
            while Present (Prim_Elmt) loop
               Prim := Node (Prim_Elmt);

               --  Skip primitives previously set entries

               if DT_Position (Prim) /= No_Uint then
                  null;

               elsif Etype (DTC_Entity (Prim)) /= RTE (RE_Tag) then
                  null;

               --  Primitives covering interface primitives are
               --  handled later

               elsif Present (Abstract_Interface_Alias (Prim)) then
                  null;

               else
                  --  Take the next available position in the DT

                  loop
                     Nb_Prim := Nb_Prim + 1;
                     exit when not Fixed_Prim (Nb_Prim);
                  end loop;

                  Set_DT_Position (Prim, UI_From_Int (Nb_Prim));
                  Fixed_Prim (Nb_Prim) := True;
               end if;

               Next_Elmt (Prim_Elmt);
            end loop;
         end;

         --  Fourth stage: Complete the decoration of primitives covering
         --  interfaces (that is, propagate the DT_Position attribute
         --  from the aliased primitive)

         Prim_Elmt := First_Prim;
         while Present (Prim_Elmt) loop
            Prim := Node (Prim_Elmt);

            if DT_Position (Prim) = No_Uint
               and then Present (Abstract_Interface_Alias (Prim))
            then
               --  Check if this entry will be placed in the primary DT

               if Etype (DTC_Entity (Abstract_Interface_Alias (Prim)))
                    = RTE (RE_Tag)
               then
                  pragma Assert (DT_Position (Alias (Prim)) /= No_Uint);
                  Set_DT_Position (Prim, DT_Position (Alias (Prim)));

               --  Otherwise it will be placed in the secondary DT

               else
                  pragma Assert
                    (DT_Position (Abstract_Interface_Alias (Prim)) /= No_Uint);

                  Set_DT_Position (Prim,
                     DT_Position (Abstract_Interface_Alias (Prim)));
               end if;
            end if;

            Next_Elmt (Prim_Elmt);
         end loop;

         --  Final stage: Ensure that the table is correct plus some further
         --  verifications concerning the primitives.

         Prim_Elmt := First_Prim;
         DT_Length := 0;

         while Present (Prim_Elmt) loop
            Prim := Node (Prim_Elmt);

            --  At this point all the primitives MUST have a position
            --  in the dispatch table

            if DT_Position (Prim) = No_Uint then
               raise Program_Error;
            end if;

            --  Calculate real size of the dispatch table

            if UI_To_Int (DT_Position (Prim)) > DT_Length then
               DT_Length := UI_To_Int (DT_Position (Prim));
            end if;

            --  Ensure that the asignated position in the dispatch
            --  table is correct

            Validate_Position (Prim);

            if Chars (Prim) = Name_Finalize then
               Finalized := True;
            end if;

            if Chars (Prim) = Name_Adjust then
               Adjusted := True;
            end if;

            --  An abstract operation cannot be declared in the private part
            --  for a visible abstract type, because it could never be over-
            --  ridden. For explicit declarations this is checked at the
            --  point of declaration, but for inherited operations it must
            --  be done when building the dispatch table. Input is excluded
            --  because

            if Is_Abstract (Typ)
              and then Is_Abstract (Prim)
              and then Present (Alias (Prim))
              and then Is_Derived_Type (Typ)
              and then In_Private_Part (Current_Scope)
              and then
                List_Containing (Parent (Prim)) =
                  Private_Declarations
                   (Specification (Unit_Declaration_Node (Current_Scope)))
              and then Original_View_In_Visible_Part (Typ)
            then
               --  We exclude Input and Output stream operations because
               --  Limited_Controlled inherits useless Input and Output
               --  stream operations from Root_Controlled, which can
               --  never be overridden.

               if not Is_TSS (Prim, TSS_Stream_Input)
                    and then
                  not Is_TSS (Prim, TSS_Stream_Output)
               then
                  Error_Msg_NE
                    ("abstract inherited private operation&" &
                     " must be overridden ('R'M 3.9.3(10))",
                    Parent (Typ), Prim);
               end if;
            end if;

            Next_Elmt (Prim_Elmt);
         end loop;

         --  Additional check

         if Is_Controlled (Typ) then
            if not Finalized then
               Error_Msg_N
                 ("controlled type has no explicit Finalize method?", Typ);

            elsif not Adjusted then
               Error_Msg_N
                 ("controlled type has no explicit Adjust method?", Typ);
            end if;
         end if;

         --  Set the final size of the Dispatch Table

         Set_DT_Entry_Count (The_Tag, UI_From_Int (DT_Length));

         --  The derived type must have at least as many components as its
         --  parent (for root types, the Etype points back to itself
         --  and the test should not fail)

         --  This test fails compiling the partial view of a tagged type
         --  derived from an interface which defines the overriding subprogram
         --  in the private part. This needs further investigation???

         if not Has_Private_Declaration (Typ) then
            pragma Assert (
              DT_Entry_Count (The_Tag) >=
              DT_Entry_Count (First_Tag_Component (Parent_Typ)));
            null;
         end if;
      end if;

      if Debug_Flag_ZZ then
         Write_DT (Typ);
      end if;
   end Set_All_DT_Position;

   -----------------------------
   -- Set_Default_Constructor --
   -----------------------------

   procedure Set_Default_Constructor (Typ : Entity_Id) is
      Loc   : Source_Ptr;
      Init  : Entity_Id;
      Param : Entity_Id;
      E     : Entity_Id;

   begin
      --  Look for the default constructor entity. For now only the
      --  default constructor has the flag Is_Constructor.

      E := Next_Entity (Typ);
      while Present (E)
        and then (Ekind (E) /= E_Function or else not Is_Constructor (E))
      loop
         Next_Entity (E);
      end loop;

      --  Create the init procedure

      if Present (E) then
         Loc   := Sloc (E);
         Init  := Make_Defining_Identifier (Loc, Make_Init_Proc_Name (Typ));
         Param := Make_Defining_Identifier (Loc, Name_X);

         Discard_Node (
           Make_Subprogram_Declaration (Loc,
             Make_Procedure_Specification (Loc,
               Defining_Unit_Name => Init,
               Parameter_Specifications => New_List (
                 Make_Parameter_Specification (Loc,
                   Defining_Identifier => Param,
                   Parameter_Type      => New_Reference_To (Typ, Loc))))));

         Set_Init_Proc (Typ, Init);
         Set_Is_Imported    (Init);
         Set_Interface_Name (Init, Interface_Name (E));
         Set_Convention     (Init, Convention_C);
         Set_Is_Public      (Init);
         Set_Has_Completion (Init);

      --  If there are no constructors, mark the type as abstract since we
      --  won't be able to declare objects of that type.

      else
         Set_Is_Abstract (Typ);
      end if;
   end Set_Default_Constructor;

   --------------
   -- Write_DT --
   --------------

   procedure Write_DT (Typ : Entity_Id) is
      Elmt : Elmt_Id;
      Prim : Node_Id;

   begin
      --  Protect this procedure against wrong usage. Required because it will
      --  be used directly from GDB

      if not (Typ in First_Node_Id .. Last_Node_Id)
        or else not Is_Tagged_Type (Typ)
      then
         Write_Str ("wrong usage: write_dt must be used with tagged types");
         Write_Eol;
         return;
      end if;

      Write_Int (Int (Typ));
      Write_Str (": ");
      Write_Name (Chars (Typ));

      if Is_Interface (Typ) then
         Write_Str (" is interface");
      end if;

      Write_Eol;

      Elmt := First_Elmt (Primitive_Operations (Typ));
      while Present (Elmt) loop
         Prim := Node (Elmt);
         Write_Str  (" - ");

         --  Indicate if this primitive will be allocated in the primary
         --  dispatch table or in a secondary dispatch table associated
         --  with an abstract interface type

         if Present (DTC_Entity (Prim)) then
            if Etype (DTC_Entity (Prim)) = RTE (RE_Tag) then
               Write_Str ("[P] ");
            else
               Write_Str ("[s] ");
            end if;
         end if;

         --  Output the node of this primitive operation and its name

         Write_Int  (Int (Prim));
         Write_Str  (": ");
         Write_Name (Chars (Prim));

         --  Indicate if this primitive has an aliased primitive

         if Present (Alias (Prim)) then
            Write_Str (" (alias = ");
            Write_Int (Int (Alias (Prim)));

            --  If the DTC_Entity attribute is already set we can also output
            --  the name of the interface covered by this primitive (if any)

            if Present (DTC_Entity (Alias (Prim)))
              and then Is_Interface (Scope (DTC_Entity (Alias (Prim))))
            then
               Write_Str  (" from interface ");
               Write_Name (Chars (Scope (DTC_Entity (Alias (Prim)))));
            end if;

            if Present (Abstract_Interface_Alias (Prim)) then
               Write_Str  (", AI_Alias of ");
               Write_Name (Chars (Scope (DTC_Entity
                                          (Abstract_Interface_Alias (Prim)))));
               Write_Char (':');
               Write_Int  (Int (Abstract_Interface_Alias (Prim)));
            end if;

            Write_Str (")");
         end if;

         --  Display the final position of this primitive in its associated
         --  (primary or secondary) dispatch table

         if Present (DTC_Entity (Prim))
           and then DT_Position (Prim) /= No_Uint
         then
            Write_Str (" at #");
            Write_Int (UI_To_Int (DT_Position (Prim)));
         end if;

         if Is_Abstract (Prim) then
            Write_Str (" is abstract;");
         end if;

         Write_Eol;

         Next_Elmt (Elmt);
      end loop;
   end Write_DT;

end Exp_Disp;
