// Copyright (C) 2024  CEA, EDF
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
//
// See http://www.salome-platform.org/ or email : webmaster.salome@opencascade.com
//

#include "GeomAnaTool_ExtractBOPFailure.hxx"

#include <BRepCheck.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <Message_Msg.hxx>
#include <NCollection_BaseAllocator.hxx>
#include <OSD_Timer.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS_AlertWithShape.hxx>

static TopTools_DataMapOfShapeListOfShape theMap;

//=======================================================================
//function : Contains
//=======================================================================
static Standard_Boolean Contains(const TopTools_ListOfShape& L,
                                 const TopoDS_Shape& S)
{
  TopTools_ListIteratorOfListOfShape it;
  for (it.Initialize(L); it.More(); it.Next()) {
    if (it.Value().IsSame(S)) {
      return Standard_True;
    }
  }
  return Standard_False;
}


//=======================================================================
//function : PrintSub
//=======================================================================
static void PrintSub(Standard_OStream& OS,
                     const BRepCheck_Analyzer& Ana,
                     const TopoDS_Shape& S,
                     const TopAbs_ShapeEnum Subtype,
                     TopTools_ListOfShape &theFailures)
{
  BRepCheck_ListIteratorOfListOfStatus itl;
  TopExp_Explorer exp;
  for (exp.Init(S,Subtype); exp.More(); exp.Next())
  {
    const Handle(BRepCheck_Result)& res = Ana.Result(exp.Current());
    const TopoDS_Shape& sub = exp.Current();
    for (res->InitContextIterator();
         res->MoreShapeInContext(); 
         res->NextShapeInContext())
    {
      if (res->ContextualShape().IsSame(S) && 
          !Contains(theMap(sub),S))
      {
        theMap(sub).Append(S);
        itl.Initialize(res->StatusOnShape());
        if (itl.Value() != BRepCheck_NoError)
        {
          if (!Contains(theFailures, sub))
          {
            theFailures.Append(sub);
          }
          //OS << "Shape faulty_" << theFailures.Extent() << " ";
          if (!Contains(theFailures, S))
          {
            theFailures.Append(S);
          }
          //OS << " on shape faulty_" << theFailures.Extent() << " :\n";
          OS << "Shape fault #" << theFailures.Extent() << ": ";
          for (;itl.More(); itl.Next())
          {
            BRepCheck::Print(itl.Value(), OS);
          }
        }
        break;
      }
    }
  }
}


//=======================================================================
//function : Print
//=======================================================================
static void Print(Standard_OStream& OS,
                  const BRepCheck_Analyzer& Ana,
                  const TopoDS_Shape& S,
                  TopTools_ListOfShape& theFailures)
{
  for (TopoDS_Iterator iter(S); iter.More(); iter.Next())
  {
    Print(OS, Ana, iter.Value(), theFailures);
  }

  TopAbs_ShapeEnum styp = S.ShapeType();
  BRepCheck_ListIteratorOfListOfStatus itl;
  if (!Ana.Result(S).IsNull() && !theMap.IsBound(S))
  {
    itl.Initialize(Ana.Result(S)->Status());
    if (itl.Value() != BRepCheck_NoError)
    {
      if (!Contains(theFailures, S))
      {
        theFailures.Append(S);
      }
      OS << "Shape fault #" << theFailures.Extent() << ": ";

      for (;itl.More(); itl.Next())
      {
        if (itl.Value() != BRepCheck_NoError)
        {
          BRepCheck::Print(itl.Value(), OS);
        }
      }
    }
  }
  if (!theMap.IsBound(S))
  {
    TopTools_ListOfShape thelist;
    theMap.Bind(S, thelist);
  }

  switch (styp)
  {
  case TopAbs_EDGE:
    PrintSub(OS, Ana,S, TopAbs_VERTEX, theFailures);
    break;
  case TopAbs_WIRE:
    PrintSub(OS, Ana, S, TopAbs_EDGE, theFailures);
    PrintSub(OS, Ana, S, TopAbs_VERTEX, theFailures);
    break;
  case TopAbs_FACE:
    PrintSub(OS, Ana, S, TopAbs_WIRE, theFailures);
    PrintSub(OS, Ana, S, TopAbs_EDGE, theFailures);
    PrintSub(OS, Ana, S, TopAbs_VERTEX, theFailures);
    break;
  case TopAbs_SHELL:
    break;
  case TopAbs_SOLID:
    PrintSub(OS, Ana, S, TopAbs_SHELL, theFailures);
    break;
  default:
    break;
  }
}


//=======================================================================
//function : ContextualDump
//purpose  : Contextual (modeling) style of output.
//=======================================================================
static void ContextualDump(Standard_OStream         &OS,
                           const BRepCheck_Analyzer &theAna,
                           const TopoDS_Shape       &theShape,
                           TopTools_ListOfShape     &theFailures)
{
  theMap.Clear();
  theFailures.Clear();

  Standard_SStream aSStream;
  Print(aSStream, theAna, theShape, theFailures);
  OS << aSStream.str();
  OS << "\n";
  theMap.Clear();

  if (!theFailures.IsEmpty())
  {
    //OS << "Faulty shapes in faulty_1 to faulty_" << theFailures.Extent() << "\n";
    OS << "=> " << theFailures.Extent() << " faulty shapes!\n";
  }

  OS << "\n";
}


GeomAnaTool_ExtractBOPFailure::GeomAnaTool_ExtractBOPFailure()
{
  myShapes.Clear();
  SetDefaults();
}


GeomAnaTool_ExtractBOPFailure::GeomAnaTool_ExtractBOPFailure(const TopTools_ListOfShape& theShapes)
{
  SetShapes(theShapes);
  SetDefaults();
}


GeomAnaTool_ExtractBOPFailure::~GeomAnaTool_ExtractBOPFailure()
{
}


void GeomAnaTool_ExtractBOPFailure::SetDefaults()
{
  myCheckGeometry = Standard_True;  // Check topology and geometry
  myUseTimer = Standard_False;      // Do not use any timer
  myShortOutput = Standard_False;   // Show error report in detailed form
  myRunParallel = Standard_False;   // Run the operation sequentially
  myExactCheck = Standard_False;    // Perform fast check
  myFailures.Clear();               // Clear the list of failed shapes
  myResult.Nullify();               // Clear the result shape
  myErrors.clear();                 // Clear the list of shape errors

  // The following options cannot be changed for this operation
  myNonDestructive = Standard_False;// Work on the shapes directly
  myUseOBB = Standard_False;        // Do not use Oriented Bounding Boxes
  myFuzzy = Precision::Confusion(); // Use the default tolerance
  myGlue = BOPAlgo_GlueOff;         // Do not glue the shapes
}


void GeomAnaTool_ExtractBOPFailure::SetShapes(const TopTools_ListOfShape& theShapes)
{
  if (theShapes.IsEmpty())
  {
    std::cout << "Error in SetShapes: the list of shapes is empty\n";
    return;
  }

  // Support passing all shapes in a single COMPOUND
  if (theShapes.Extent() == 1)
  {
    TopoDS_Shape aShape = theShapes.First();
    if (aShape.ShapeType() == TopAbs_COMPOUND)
    {
      myShapes.Clear();
      TopTools_ListOfShape theShapes;
      for (TopoDS_Iterator it(aShape); it.More(); it.Next())
      {
        myShapes.Append(it.Value());
      }
      return;
    }
    else
    {
      std::cout << "Error in SetShapes: the single given shape must be a COMPOUND\n";
      return;
    }
  }

  myShapes = theShapes;
}


const TopTools_ListOfShape& GeomAnaTool_ExtractBOPFailure::Shapes() const
{
  return myShapes;
}


void GeomAnaTool_ExtractBOPFailure::SetCheckGeometry(const Standard_Boolean aFlag)
{
  myCheckGeometry = aFlag;
}


Standard_Boolean GeomAnaTool_ExtractBOPFailure::CheckGeometry() const
{
  return myCheckGeometry;
}


void GeomAnaTool_ExtractBOPFailure::SetUseTimer(const Standard_Boolean aFlag)
{
  myUseTimer = aFlag;
}


Standard_Boolean GeomAnaTool_ExtractBOPFailure::UseTimer() const
{
  return myUseTimer;
}


void GeomAnaTool_ExtractBOPFailure::SetShortOutput(const Standard_Boolean aFlag)
{
  myShortOutput = aFlag;
}


Standard_Boolean GeomAnaTool_ExtractBOPFailure::ShortOutput() const
{
  return myShortOutput;
}


void GeomAnaTool_ExtractBOPFailure::SetRunParallel(const Standard_Boolean aFlag)
{
  myRunParallel = aFlag;
}


Standard_Boolean GeomAnaTool_ExtractBOPFailure::RunParallel() const
{
  return myRunParallel;
}


void GeomAnaTool_ExtractBOPFailure::SetExactCheck(const Standard_Boolean aFlag)
{
  myExactCheck = aFlag;
}


Standard_Boolean GeomAnaTool_ExtractBOPFailure::ExactCheck() const
{
  return myExactCheck;
}


Standard_Boolean GeomAnaTool_ExtractBOPFailure::HasFailures() const
{
  return (myFailures.Extent() > 0);
}


const TopTools_ListOfShape& GeomAnaTool_ExtractBOPFailure::Failures() const
{
  return myFailures;
}


const std::list<GeomAnaTool::ShapeError>& GeomAnaTool_ExtractBOPFailure::ShapeErrors() const
{
  return myErrors;
}


const TopoDS_Shape& GeomAnaTool_ExtractBOPFailure::Result() const
{
  return myResult;
}


Standard_Integer GeomAnaTool_ExtractBOPFailure::Perform_bfillds()
{
  Standard_Integer aNbShapes = myShapes.Extent();
  if (aNbShapes == 0)
  {
    std::cout << "No shapes to process (err=101)\n";
    return 101;
  }

  Handle(NCollection_BaseAllocator) aAllocator = NCollection_BaseAllocator::CommonBaseAllocator();
  myPaveFiller = std::make_shared<BOPAlgo_PaveFiller>(aAllocator);
  if (!myPaveFiller.get())
  {
    std::cout << "BOPAlgo_PaveFiller - Cannot create the pave filler (err=102)\n";
    return 102;
  }
  myPaveFiller->SetArguments(myShapes);
  myPaveFiller->SetRunParallel(myRunParallel);
  myPaveFiller->SetNonDestructive(myNonDestructive);
  myPaveFiller->SetFuzzyValue(myFuzzy);
  myPaveFiller->SetGlue(myGlue);
  myPaveFiller->SetUseOBB(myUseOBB);

  OSD_Timer aTimer;
  aTimer.Start();
  myPaveFiller->Perform();
  ReportAlerts(myPaveFiller->GetReport());
  if (myPaveFiller->HasErrors())
  {
    std::cout << "Error in pave filler operation (err=103)\n";
    return 103;
  }
  aTimer.Stop();
  if (myUseTimer)
  {
    char buf[128];
    Sprintf(buf, "PaveFiller - Tps: %7.2lf\n", aTimer.ElapsedTime());
    std::cout << buf;
  }

  return 0;
}


Standard_Integer GeomAnaTool_ExtractBOPFailure::Perform_bbuild()
{
  BOPDS_PDS pDS = myPaveFiller->PDS();
  if (!pDS)
  {
    std::cout << "BOPDS_DS - Prepare PaveFiller first (err=201)\n";
    return 201;
  }

  Handle(NCollection_BaseAllocator) aAllocator = NCollection_BaseAllocator::CommonBaseAllocator();
  myBuilder = std::make_shared<BOPAlgo_Builder>(aAllocator);
  if (!myBuilder.get())
  {
    std::cout << "BOPAlgo_Builder - Cannot create the builder (err=202)\n";
    return 202;
  }
  myBuilder->Clear();

  // Add all shapes to the builder
  TopTools_ListIteratorOfListOfShape aIt;
  for (aIt.Initialize(myShapes); aIt.More(); aIt.Next())
  {
    const TopoDS_Shape& aShape = aIt.Value();
    myBuilder->AddArgument(aShape);
  }

  // Set all builder options
  myBuilder->SetRunParallel(myRunParallel);
  myBuilder->SetCheckInverted(Standard_True);
  myBuilder->SetToFillHistory(Standard_False);

  OSD_Timer aTimer;
  aTimer.Start();
  myBuilder->PerformWithFiller(*myPaveFiller); 
  ReportAlerts(myBuilder->GetReport());

  // // Set history of GF operation into the session
  // if (BRepTest_Objects::IsHistoryNeeded())
  //   BRepTest_Objects::SetHistory(aPF.Arguments(), aBuilder);

  if (myBuilder->HasErrors())
  {
    std::cout << "Error in builder operation (err=203)\n";
    return 203;
  }
  aTimer.Stop();
  if (myUseTimer)
  {
    char buf[128];
    Sprintf(buf, "Builder - Tps: %7.2lf\n", aTimer.ElapsedTime());
    std::cout << buf;
  }

  myResult = myBuilder->Shape();
  if (myResult.IsNull())
  {
    std::cout << "Result is a null shape (err=204)\n";
    return 204;
  }

  return 0;
}


Standard_Integer GeomAnaTool_ExtractBOPFailure::Perform_checkshape()
{
  if (myResult.IsNull()) 
  {
    std::cout << "The result shape is not a topological shape (err=301)!\n";
    return 301;
  }

  // Standard_Boolean IsContextDump = Standard_True;

  try
  {
    OCC_CATCH_SIGNALS
    BRepCheck_Analyzer anAna (myResult, myCheckGeometry, myRunParallel, myExactCheck);
    Standard_Boolean   isValid = anAna.IsValid();

    Standard_OStream &OS = std::cout;

    if (isValid)
    {
      //if (IsContextDump)
      {
        OS << "This shape seems to be valid\n";
      }
      // else
      // {
      //   std::cout << " -- The Shape " << aShapeName << " looks OK\n";
      // }
    }
    else
    {
      GeomAnaTool::FillErrors(anAna, myResult, myErrors);

      if (myShortOutput)
      {
        OS << "This shape has faulty shapes\n";
      }
      else
      {
        std::cout << "------ Dump the failures:\n";
        //if (IsContextDump)
        {
          ContextualDump(OS, anAna, myResult, myFailures);
        }
        // else
        // {
        //   StructuralDump(OS, anAna, aShapeName, aPref, aShape);
        // }
      }
    }
  }
  catch (Standard_Failure const& anException)
  {
    std::cout << "checkshape exception : ";
    std::cout << anException.GetMessageString();
    std::cout << "\n";
    return 302;
  }

  return 0;
}


void GeomAnaTool_ExtractBOPFailure::Perform()
{
  // Reset the error code and clear the list of failed shapes
  Standard_Integer aErrorCode = 0;
  myFailures.Clear();


  if (0 != (aErrorCode = Perform_bfillds()))
  {
    return;
  }

  if (0 != (aErrorCode = Perform_bbuild()))
  {
    return;
  }

  if (0 != (aErrorCode = Perform_checkshape()))
  {
    return;
  }
}


void GeomAnaTool_ExtractBOPFailure::ReportAlerts(const Handle(Message_Report)& theReport)
{
  // first report warnings, then errors
  Message_Gravity anAlertTypes[2] = { Message_Warning, Message_Fail };
  TCollection_ExtendedString aMsgType[2] = { "Warning: ", "Error: " };
  for (int iGravity = 0; iGravity < 2; iGravity++)
  {
    // report shapes for the same type of alert together
    NCollection_Map<Handle(Standard_Transient)> aPassedTypes;
    const Message_ListOfAlert& aList = theReport->GetAlerts (anAlertTypes[iGravity]);
    for (Message_ListOfAlert::Iterator aIt (aList); aIt.More(); aIt.Next())
    {
      // check that this type of warnings has not yet been processed
      const Handle(Standard_Type)& aType = aIt.Value()->DynamicType();
      if (!aPassedTypes.Add(aType))
        continue;

      // get alert message
      Message_Msg aMsg (aIt.Value()->GetMessageKey());
      TCollection_ExtendedString aText = aMsgType[iGravity] + aMsg.Get();

      // collect all shapes if any attached to this alert
      bool bWarnShapes = false;
      if (bWarnShapes)
      {
        TCollection_AsciiString aShapeList;
        Standard_Integer aNbShapes = 0;
        for (Message_ListOfAlert::Iterator aIt2 (aIt); aIt2.More(); aIt2.Next())
        {
          Handle(TopoDS_AlertWithShape) aShapeAlert = Handle(TopoDS_AlertWithShape)::DownCast (aIt2.Value());

          if (!aShapeAlert.IsNull() &&
              (aType == aShapeAlert->DynamicType()) &&
              !aShapeAlert->GetShape().IsNull())
          {
            //
            char aName[80];
            Sprintf(aName, "%ss_%d_%d", (iGravity ? "e" : "w"), aPassedTypes.Extent(), ++aNbShapes);
            //DBRep::Set(aName, aShapeAlert->GetShape());
            //
            aShapeList += " ";
            aShapeList += aName;
          }
        }
        aText += (aNbShapes ? ": " : "(no shapes attached)");
        aText += aShapeList;
      }

      // output message with list of shapes
      std::cout << aText << "\n";
    }
  }
}
