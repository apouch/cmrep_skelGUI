#include "MouseInteractor.h"	
#include <cmath>
#include <string>

#ifndef DBL_MAX
#define DBL_MAX 1e100
#endif

//Initialize the global value
MouseInteractor::MouseInteractor():vectorTagInfo(Global::vectorTagInfo), vectorTagTriangles(Global::vectorTagTriangles), vectorTagPoints(Global::vectorTagPoints), 
	vectorTagEdges(Global::vectorTagEdges), labelData(Global::labelData), triNormalActors(Global::triNormalActors),
	isSkeleton(Global::isSkeleton), selectedTag(Global::selectedTag), targetReduction(Global::targetReduction), featureAngle(Global::featureAngle), 
	tagRadius(Global::tagRadius), decimateMode(Global::decimateMode)
{
	selectedTriangle = NULL;
	Global::backCol[0] = 0.4; Global::backCol[1] = 0.4; Global::backCol[2] = 0.4;
	backCol[0] = 0.4; backCol[1] = 0.4; backCol[2] = 0.4;
	drawTriMode = false;
	decimateMode = false;
	tagRadius = 1.0;
	//Default operation is VIEW
	preOperationFlag = operationFlag = VIEW;
	emit operationChanged(VIEW);
	skelState = meshState = SHOW;
	preKey = "";
	actionCounter = -1;
	isCtrlPress = false;
	isShiftPress = false;
	movePtIndex = -1;
}

//Handle mouse event
void MouseInteractor::OnLeftButtonDown()
{
	int* clickPos = new int[2];
	clickPos = this->GetInteractor()->GetEventPosition();
	// Pick from this location.
	vtkSmartPointer<vtkCellPicker>  picker =
		vtkSmartPointer<vtkCellPicker>::New();

	int con = 1;
	int sucessPick = picker->Pick(clickPos[0], clickPos[1], 0, this->GetDefaultRenderer());
	//Picking is successful
	if(sucessPick != 0)
	{
		double* pos = new double[3];
		pos = picker->GetPickPosition();		
		vtkRenderWindowInteractor *rwi = this->Interactor;
		//To see if a skeleton, use radius in VTK file to define whether is a skeleton
		if(isSkeleton)
		{
			std::string key;
			if(rwi->GetKeySym()!= NULL)
				key = rwi->GetKeySym();
			else
				key = "";

			if( (key.compare("a") == 0 || key.compare("A") == 0 || operationFlag == ADDPOINT) && pos[0] != 0 && pos[1] != 0 && pos[2] != 0)
            {
				reset();				
				TagPoint tagpt = AddPoint(pos);
				if(tagpt.actor != NULL)
					DoAction(ADDPOINT, tagpt, tagpt.ptIndex);
			}				
			else if(key.compare("e") == 0 || key.compare("E") == 0 || operationFlag == DELETEPOINT)
			{
				//For delete point event
				reset();
				TagPoint tagpt = DeletePoint(pos);	
				if(tagpt.actor != NULL){
					DoAction(DELETEPOINT, tagpt, tagpt.ptIndex);
				}
			}								
			else if(key.compare("t") == 0 || key.compare("T") == 0 || operationFlag == CREATETRI)
			{
				drawTriMode = true;
				//Do action part has been written in PickPointForTri function
				con = PickPointForTri(pos);
				if(con == 0){
					QMessageBox messageBox;
					messageBox.critical(0,"Error","Triangle Violation: " + QString::fromStdString(recordTriViolation));
					int sizeT = vectorActions.size() - 1;
					for(int i = sizeT; i > sizeT - 2; i--)
					{
						if(vectorActions[i].action == PICKPTTRI)
							vectorActions.erase(vectorActions.end() - 1);
						else
							break;
					}
				}
			}
			else if(key.compare("n") == 0 || key.compare("N") == 0 || operationFlag == FLIPNORMAL)
			{
				reset();
				if(FlipNormal(pos))
					DoAction(FLIPNORMAL, pos);
			}
			else if(key.compare("d") == 0 || key.compare("D") == 0 || operationFlag == DELETETRI)
			{
				reset();
				TagTriangle tagTri = DeleteTriangle(pos);	
				if(tagTri.triActor != NULL)
					DoAction(DELETETRI, tagTri);
			}
			else if(key.compare("l") == 0 || key.compare("L") == 0 || operationFlag == CHANGETRILABEL)
			{
				reset();
				int preIndex = ChangeTriLabel(pos);
				if(preIndex != -1)
					DoAction(CHANGETRILABEL, pos, preIndex);
			}
			else if(key.compare("o") == 0 || key.compare("O") == 0 || operationFlag == MOVEPT)
			{
				SelectMovePt(pos);
			}
            else if(operationFlag == EDITTAGPT)
            {
                reset();
                TagPoint tagpt = ChangePtLabel(pos);
                if(tagpt.actor != NULL){
                    DoAction(EDITTAGPT, tagpt, tagpt.typeIndex);
                }
            }
		}//end of key press
	}
	/*else
	{
		if (operationFlag == UNDO)
		{
			std::cout << "Undo" << std::endl;
			reset();
			UndoAction();
		}

		else if (operationFlag == REDO)
		{
			std::cout << "Redo" << std::endl;
			reset();
			RedoAction();
		}
	}*/
	if(con != 0)
		vtkInteractorStyleTrackballCamera::OnLeftButtonDown();		
}

void MouseInteractor::OnMiddleButtonDown()
{
	//For moving point action
	if(movePtIndex != -1)
	{
		int* clickPos = new int[2];
		clickPos = this->GetInteractor()->GetEventPosition();	
		vtkSmartPointer<vtkCellPicker>  picker =
			vtkSmartPointer<vtkCellPicker>::New();

		int sucessPick = picker->Pick(clickPos[0], clickPos[1], 0, this->GetDefaultRenderer());
		if(sucessPick != 0)//pick successful
		{
			double* pos = new double[3];
			pos = picker->GetPickPosition();
			MovePoint(pos);			
		}
	}
	vtkInteractorStyleTrackballCamera::OnMiddleButtonDown();
}

void MouseInteractor::SelectMovePt(double pos[3])
{
	for(int i = 0; i < vectorTagPoints.size(); i++)
	{
		TagPoint at = vectorTagPoints[i];
		double* actorPos = new double [3];
		actorPos = vectorTagPoints[i].actor->GetCenter();
		double deltaDis = Distance(pos, actorPos);
		if(deltaDis <= tagRadius){
			//Reset the color of previous selected point
			if(movePtIndex != -1)
			{
				TagPoint at = vectorTagPoints[movePtIndex];
				vectorTagPoints[movePtIndex].actor->GetProperty()->SetColor(vectorTagInfo[at.comboBoxIndex].tagColor[0] / 255.0,
					vectorTagInfo[at.comboBoxIndex].tagColor[1] / 255.0, 
					vectorTagInfo[at.comboBoxIndex].tagColor[2] / 255.0);
			}
			//Set point color
			vectorTagPoints[i].actor->GetProperty()->SetColor(0,0,0);
			movePtIndex = i;
			break;
		}
	}
}

bool MouseInteractor::MovePoint(double pos[3])
{
	//Find the first actor
	vtkSmartPointer<vtkActorCollection> actors = this->GetDefaultRenderer()->GetActors();
	vtkSmartPointer<vtkActor> actor0 =  static_cast<vtkActor *>(actors->GetItemAsObject(0));	
	vtkSmartPointer<vtkDataSet> vtkdata = actor0->GetMapper()->GetInputAsDataSet();
	vtkDoubleArray* radiusArray = (vtkDoubleArray*)vtkdata->GetPointData()->GetArray("Radius");
	double minDistance = DBL_MAX;
	double finalPos[3] = {};
	double pointRadius=0.0;
	int pointSeq=0;
	for(vtkIdType i = 0; i < vtkdata->GetNumberOfPoints(); i++)
	{
		double p[3];
		vtkdata->GetPoint(i,p);
		double dist = Distance(pos, p);
		if(dist < minDistance){
			minDistance = dist;
			pointSeq = i;
			finalPos[0] = p[0]; finalPos[1] = p[1]; finalPos[2] = p[2];
			pointRadius = radiusArray->GetValue(i);
		}
	}

	for(int i = 0; i < vectorTagPoints.size(); i++)
	{
		if(vectorTagPoints[i].seq == pointSeq)
			return false;
	}

	if(vectorTagPoints[movePtIndex].seq != pointSeq)
	{
		DoAction(MOVEPT, movePtIndex, vectorTagPoints[movePtIndex].seq);

		for(int i = 0; i < vectorTagTriangles.size(); i++)
		{
			if(vectorTagTriangles[i].seq1 == vectorTagPoints[movePtIndex].seq)
			{				
				MoveTriangle(finalPos, i, 1, pointSeq);
			}
			else if(vectorTagTriangles[i].seq2 == vectorTagPoints[movePtIndex].seq)
			{
				MoveTriangle(finalPos, i, 2, pointSeq);
			}
			else if(vectorTagTriangles[i].seq3 == vectorTagPoints[movePtIndex].seq)
			{
				MoveTriangle(finalPos, i, 3, pointSeq);
			}
		}

		//Change the history point seq, For undo feature
		for(int i = 0; i < vectorActions.size(); i ++)
		{
			if(vectorActions[i].action == DELETEPOINT || vectorActions[i].action == ADDPOINT)
			{
				if(vectorActions[i].pointInfo.seq == vectorTagPoints[movePtIndex].seq)
				{
					vectorActions[i].pointInfo.seq = pointSeq;
				}
			}
		}
		//Update the point sequence, position and radius
		vectorTagPoints[movePtIndex].seq = pointSeq;
		vectorTagPoints[movePtIndex].pos[0] = finalPos[0];
		vectorTagPoints[movePtIndex].pos[1] = finalPos[1];
		vectorTagPoints[movePtIndex].pos[2] = finalPos[2];
		vectorTagPoints[movePtIndex].radius = pointRadius;
		//Remove previous point
		this->GetDefaultRenderer()->RemoveActor(vectorTagPoints[movePtIndex].actor);

		//Create a sphere
		vtkSmartPointer<vtkSphereSource> sphereSource =
			vtkSmartPointer<vtkSphereSource>::New();
		sphereSource->SetCenter(finalPos[0], finalPos[1], finalPos[2]);
		sphereSource->SetRadius(tagRadius);

		//Create a mapper and actor
		vtkSmartPointer<vtkPolyDataMapper> mapper =
			vtkSmartPointer<vtkPolyDataMapper>::New();
		mapper->SetInputConnection(sphereSource->GetOutputPort());

		TagInfo ti = vectorTagInfo[vectorTagPoints[movePtIndex].comboBoxIndex];
		vtkSmartPointer<vtkActor> actor =
			vtkSmartPointer<vtkActor>::New();
		actor->SetMapper(mapper);
		actor->GetProperty()->SetColor(0,0,0);

		vectorTagPoints[movePtIndex].actor = actor;
		this->GetDefaultRenderer()->AddActor(actor);
		return true;
	}
	return false;
}

void MouseInteractor::MovePoint(int ptIndex, int oldSeq)
{
	//Mainly for undo function
	vtkSmartPointer<vtkActorCollection> actors = this->GetDefaultRenderer()->GetActors();
	vtkSmartPointer<vtkActor> actor0 =  static_cast<vtkActor *>(actors->GetItemAsObject(0));	
	vtkSmartPointer<vtkDataSet> vtkdata = actor0->GetMapper()->GetInputAsDataSet();
	vtkDoubleArray* radiusArray = (vtkDoubleArray*)vtkdata->GetPointData()->GetArray("Radius");

	double finalPos[3];
	double pointRadius;
	vtkdata->GetPoint(oldSeq,finalPos);
	pointRadius = radiusArray->GetValue(oldSeq);

	//update triangle
	for(int i = 0; i < vectorTagTriangles.size(); i++)
	{
		if(vectorTagTriangles[i].seq1 == vectorTagPoints[ptIndex].seq)
		{				
			MoveTriangle(finalPos, i, 1, oldSeq);
		}
		else if(vectorTagTriangles[i].seq2 == vectorTagPoints[ptIndex].seq)
		{
			MoveTriangle(finalPos, i, 2, oldSeq);
		}
		else if(vectorTagTriangles[i].seq3 == vectorTagPoints[ptIndex].seq)
		{
			MoveTriangle(finalPos, i, 3, oldSeq);
		}
	}

	//update history actions
	for(int i = 0; i < vectorActions.size(); i ++)
	{
		if(vectorActions[i].action == DELETEPOINT || vectorActions[i].action == ADDPOINT)
		{
			if(vectorActions[i].pointInfo.seq == vectorTagPoints[ptIndex].seq)
			{
				vectorActions[i].pointInfo.seq = oldSeq;
			}
		}
	}

	vectorTagPoints[ptIndex].seq = oldSeq;
	vectorTagPoints[ptIndex].pos[0] = finalPos[0];
	vectorTagPoints[ptIndex].pos[1] = finalPos[1];
	vectorTagPoints[ptIndex].pos[2] = finalPos[2];
	vectorTagPoints[ptIndex].radius = pointRadius;

	this->GetDefaultRenderer()->RemoveActor(vectorTagPoints[ptIndex].actor);
	
	//Create a sphere
	vtkSmartPointer<vtkSphereSource> sphereSource =
		vtkSmartPointer<vtkSphereSource>::New();
	sphereSource->SetCenter(finalPos[0], finalPos[1], finalPos[2]);
	sphereSource->SetRadius(tagRadius);

	//Create a mapper and actor
	vtkSmartPointer<vtkPolyDataMapper> mapper =
		vtkSmartPointer<vtkPolyDataMapper>::New();
	mapper->SetInputConnection(sphereSource->GetOutputPort());

	TagInfo ti = vectorTagInfo[vectorTagPoints[ptIndex].comboBoxIndex];
	TagPoint at = vectorTagPoints[ptIndex];
	vtkSmartPointer<vtkActor> actor =
		vtkSmartPointer<vtkActor>::New();
	actor->SetMapper(mapper);
	actor->GetProperty()->SetColor(vectorTagInfo[at.comboBoxIndex].tagColor[0] / 255.0,
		vectorTagInfo[at.comboBoxIndex].tagColor[1] / 255.0, 
		vectorTagInfo[at.comboBoxIndex].tagColor[2] / 255.0);

	vectorTagPoints[ptIndex].actor = actor;
	this->GetDefaultRenderer()->AddActor(actor);
}

bool MouseInteractor::MoveTriangle(double pos[3], int triIndex, int num, int newSeq)
{
	if(num == 1)
	{
		for(int i = 0; i < vectorActions.size(); i++)
		{
			if(vectorActions[i].action == CREATETRI || vectorActions[i].action == DELETETRI){
				if(vectorTagTriangles[triIndex].seq1 == vectorActions[i].triangleInfo.seq1){
					vectorActions[i].triangleInfo.seq1 = newSeq;
				}
			}
		}
		vectorTagTriangles[triIndex].seq1 = newSeq;
		vectorTagTriangles[triIndex].p1[0] = pos[0];
		vectorTagTriangles[triIndex].p1[1] = pos[1];
		vectorTagTriangles[triIndex].p1[2] = pos[2];
		
	}
	else if(num == 2)
	{
		for(int i = 0; i < vectorActions.size(); i++)
		{
			if(vectorActions[i].action == CREATETRI || vectorActions[i].action == DELETETRI){
				if(vectorTagTriangles[triIndex].seq2 == vectorActions[i].triangleInfo.seq2){
					vectorActions[i].triangleInfo.seq2 = newSeq;
				}
			}
		}
		vectorTagTriangles[triIndex].seq2 = newSeq;
		vectorTagTriangles[triIndex].p2[0] = pos[0];
		vectorTagTriangles[triIndex].p2[1] = pos[1];
		vectorTagTriangles[triIndex].p2[2] = pos[2];
	}
	else if(num == 3)
	{
		for(int i = 0; i < vectorActions.size(); i++)
		{
			if(vectorActions[i].action == CREATETRI || vectorActions[i].action == DELETETRI){
				if(vectorTagTriangles[triIndex].seq3 == vectorActions[i].triangleInfo.seq3){
					vectorActions[i].triangleInfo.seq3 = newSeq;
				}
			}
		}
		vectorTagTriangles[triIndex].seq3 = newSeq;
		vectorTagTriangles[triIndex].p3[0] = pos[0];
		vectorTagTriangles[triIndex].p3[1] = pos[1];
		vectorTagTriangles[triIndex].p3[2] = pos[2];
	}

	this->GetDefaultRenderer()->RemoveActor(vectorTagTriangles[triIndex].triActor);

	vtkSmartPointer<vtkPoints> pts =
		vtkSmartPointer<vtkPoints>::New();
	pts->InsertNextPoint(vectorTagTriangles[triIndex].p1);
	pts->InsertNextPoint(vectorTagTriangles[triIndex].p2);
	pts->InsertNextPoint(vectorTagTriangles[triIndex].p3);

	vtkSmartPointer<vtkTriangle> triangle =
		vtkSmartPointer<vtkTriangle>::New();
	triangle->GetPointIds()->SetId ( 0, 0 );
	triangle->GetPointIds()->SetId ( 1, 1 );
	triangle->GetPointIds()->SetId ( 2, 2 );

	vtkSmartPointer<vtkCellArray> triangles =
		vtkSmartPointer<vtkCellArray>::New();
	triangles->InsertNextCell ( triangle );

	// Create a polydata object
	vtkSmartPointer<vtkPolyData> trianglePolyData =
		vtkSmartPointer<vtkPolyData>::New();

	// Add the geometry and topology to the polydata
	trianglePolyData->SetPoints ( pts );
	trianglePolyData->SetPolys ( triangles );

	// Create mapper and actor
	vtkSmartPointer<vtkPolyDataMapper> mapper =
		vtkSmartPointer<vtkPolyDataMapper>::New();
#if VTK_MAJOR_VERSION <= 5
	mapper->SetInput(trianglePolyData);
	//mapper->SetInput(appendFilter->GetOutput());
#else
	mapper->SetInputData(trianglePolyData);
#endif
	vtkSmartPointer<vtkActor> actor =
		vtkSmartPointer<vtkActor>::New();
	actor->SetMapper(mapper);
	actor->GetProperty()->SetEdgeVisibility(true);
	actor->GetProperty()->SetEdgeColor(0.0,0.0,0.0);
	actor->GetProperty()->SetColor(triLabelColors[vectorTagTriangles[triIndex].index].red()/255.0,
		triLabelColors[vectorTagTriangles[triIndex].index].green()/255.0,
		triLabelColors[vectorTagTriangles[triIndex].index].blue()/255.0);
	vtkSmartPointer<vtkProperty> backPro = 
		vtkSmartPointer<vtkProperty>::New();
	backPro->SetColor(backCol);
	actor->SetBackfaceProperty(backPro);

	//update undo history triangle actor
	for(int i = 0; i < vectorActions.size(); i++)
	{
		if(vectorActions[i].action == CREATETRI || vectorActions[i].action == DELETETRI)
			if(vectorActions[i].triangleInfo.seq1 == vectorTagTriangles[triIndex].seq1 &&
				vectorActions[i].triangleInfo.seq2 == vectorTagTriangles[triIndex].seq2 &&
				vectorActions[i].triangleInfo.seq3 == vectorTagTriangles[triIndex].seq3)
			{
				vectorActions[i].triangleInfo.triActor = actor;
				break;
			}
	}
	vectorTagTriangles[triIndex].triActor = actor;
	this->GetDefaultRenderer()->AddActor(actor);	

	return true;
}

bool isKeyNeeded(std::string key)
{
    if(key.compare("a") == 0 || key.compare("A") == 0 ||
		key.compare("e") == 0 || key.compare("E") == 0 ||
		key.compare("t") == 0 || key.compare("T") == 0 ||
		key.compare("n") == 0 || key.compare("N") == 0 ||
		key.compare("d") == 0 || key.compare("D") == 0 ||
		key.compare("v") == 0 || key.compare("V") == 0 ||
		key.compare("h") == 0 || key.compare("H") == 0 ||
		key.compare("m") == 0 || key.compare("M") == 0 ||
		key.compare("o") == 0 || key.compare("O") == 0 ||
		key.compare("z") == 0 || key.compare("Z") == 0)
	{
		return true;
	}
	return false;
}

void MouseInteractor::OnKeyRelease()
{
	std::string key = this->Interactor->GetKeySym();
	if(key.compare("t") != 0 && key.compare("T") != 0){
		this->Interactor->SetKeySym("");			
	}
	
	if(key.compare("Control_L") == 0 || key.compare("Control_R") == 0)
		isCtrlPress = false;

	if (key.compare("Shift_L") == 0 || key.compare("Shift_R") == 0)
		isShiftPress = false;

	if(isKeyNeeded(key) && key.compare("Control_L") != 0 && key.compare("c") != 0 && isSkeleton){
		operationFlag = preOperationFlag;
		emit operationChanged(operationFlag);
	}
}


//To emit operation signal when key pressed
void MouseInteractor::OnKeyPress()
{
	vtkRenderWindowInteractor *rwi = this->Interactor;
	std::string key = rwi->GetKeySym();

	if(key.compare("Control_L") == 0 || key.compare("Control_R") == 0)
		isCtrlPress = true;
	if (key.compare("Shift_L") == 0 || key.compare("Shift_R") == 0)
		isShiftPress = true;

    //std::cout<< ((key.compare("z") == 0) && isCtrlPress && isShiftPress ) <<std::endl;
//    std::cout<<isSkeleton<<std::endl;
//    std::cout<<(key.compare("Control_L")!=0)<<std::endl;
//    std::cout<<(key.compare("c")!=0)<<std::endl;
	if(isSkeleton && isKeyNeeded(key)  && key.compare("Control_L") != 0 && key.compare("c") != 0) 
	{
//        std::cout<<"True"<<std::endl;
		if(preKey.compare(key) != 0)
			preOperationFlag = operationFlag;
		preKey = key;

		if(key.compare("a") == 0 || key.compare("A") == 0){
			operationFlag = ADDPOINT;
			emit operationChanged(ADDPOINT);
		}
		else if(key.compare("e") == 0 || key.compare("E") == 0){
			operationFlag = DELETEPOINT;
			emit operationChanged(DELETEPOINT);
		}
		else if(key.compare("t") == 0 || key.compare("T") == 0){
			operationFlag = CREATETRI;
			emit operationChanged(CREATETRI);
		}
		else if(key.compare("n") == 0 || key.compare("N") == 0){
			operationFlag = FLIPNORMAL;
			emit operationChanged(FLIPNORMAL);
		}
		else if(key.compare("d") == 0 || key.compare("D") == 0){
			operationFlag = DELETETRI;
			emit operationChanged(DELETETRI);
		}
		else if(key.compare("v") == 0 || key.compare("V") == 0){
			operationFlag = VIEW;
			emit operationChanged(VIEW);
		}
		else if(key.compare("l") == 0 || key.compare("L") == 0){
			operationFlag = CHANGETRILABEL;
			emit operationChanged(CHANGETRILABEL);
		}
		else if(key.compare("o") == 0 || key.compare("O") == 0){
			operationFlag = MOVEPT;
			emit operationChanged(MOVEPT);
		}
		else if(key.compare("h") == 0 || key.compare("H") == 0)
		{
			if(skelState == HIDE){
				skelState = SHOW;
				emit skelStateChanged(SHOW);
			}
			else{
				skelState = HIDE;
				emit skelStateChanged(HIDE);
			}
		}
		else if(key.compare("m") == 0 || key.compare("M") == 0)
		{
			if(meshState == HIDE){
				meshState = SHOW;
				emit meshStateChanged(SHOW);
			}
			else{
				meshState = HIDE;
				emit meshStateChanged(HIDE);
			}
		}
		else if((key.compare("z") == 0) && isCtrlPress && !isShiftPress)
		{
			//std::cout << "Undo" << std::endl;
			UndoAction();
		}
		else if ((key.compare("z") == 0) && isCtrlPress && isShiftPress)
		{
			//std::cout << "Redo" << std::endl;
			RedoAction();
		}
		
	}
	vtkInteractorStyleTrackballCamera::OnKeyPress();
}

double MouseInteractor::Distance(double p1[3], double p2[3]){
	return std::sqrt(std::pow(p1[0] - p2[0],2) + std::pow(p1[1] - p2[1], 2) + std::pow(p1[2] - p2[2], 2));
}


//Set constrian based on different combination of point type
int MouseInteractor::ConstrainEdge(int type1, int type2)
{
	// 1 = Branch point  2 = Free Edge point 3 = Interior point  4 = others
	//return 1;
	if(type1 == 1 && type2 == 1)
		return 3;
	else if(type1 == 2 && type2 == 2)
		return 1;
	else if(type1 == 3 && type2 == 3)
		return 2;
	else if((type1 == 1 && type2 == 2) || (type1 == 2 && type2 == 1))
		return 2;
	else if((type1 == 1 && type2 == 3) || (type1 == 3 && type2 == 1))
		return 2;
	else if((type1 == 2 && type2 == 3) || (type1 == 3 && type2 == 2))
		return 2;
}

int MouseInteractor::PairNumber(int a, int b){
	int a1 = std::min(a,b);
	int b1 = std::max(a,b);
	return (a1 + b1) * (a1 + b1 + 1) / 2.0 + b1;
}

void MouseInteractor::setNormalGenerator(vtkSmartPointer<vtkPolyDataNormals> normalGenerator)
{
	this->normalGenerator = normalGenerator;
}


//Check if the Normal is pointing to the right direction, if not, change the sequence of point for the triangle
void MouseInteractor::CheckNormal(int triPtIds[3])
{
	int id1 = triPtIds[0], id2 = triPtIds[1], id3 = triPtIds[2];
	vtkSmartPointer<vtkPolyData> normalPolyData = normalGenerator->GetOutput();
	
	vtkFloatArray* normalDataFloat =
		vtkFloatArray::SafeDownCast(normalPolyData->GetPointData()->GetArray("Normals"));
	float result[3];
	if(normalDataFloat)
	{
		int nc = normalDataFloat->GetNumberOfTuples();
		float *normal1 = new float[3];
        normalDataFloat->GetTypedTuple(vectorTagPoints[id1].seq, normal1);
		float *normal2 = new float[3];
        normalDataFloat->GetTypedTuple(vectorTagPoints[id2].seq, normal2);
		float *normal3 = new float[3];
        normalDataFloat->GetTypedTuple(vectorTagPoints[id2].seq, normal3);

		float normalAverage[3];
		normalAverage[0] = (normal1[0] + normal2[0] + normal3[0]) / 3.0f;
		normalAverage[1] = (normal1[1] + normal2[1] + normal3[1]) / 3.0f;
		normalAverage[2] = (normal1[2] + normal2[2] + normal3[2]) / 3.0f;

		float d1[3];
		d1[0] = vectorTagPoints[id2].pos[0] - vectorTagPoints[id1].pos[0];
		d1[1] = vectorTagPoints[id2].pos[1] - vectorTagPoints[id1].pos[1];
		d1[2] = vectorTagPoints[id2].pos[2] - vectorTagPoints[id1].pos[2];
		float d2[3];
		d2[0] = vectorTagPoints[id3].pos[0] - vectorTagPoints[id2].pos[0];
		d2[1] = vectorTagPoints[id3].pos[1] - vectorTagPoints[id2].pos[1];
		d2[2] = vectorTagPoints[id3].pos[2] - vectorTagPoints[id2].pos[2];
				
		vtkMath::Cross(d1, d2, result);
		vtkMath::Normalize(result);
		vtkMath::Normalize(normalAverage);

		float cos = vtkMath::Dot(result, normalAverage);

		if(cos < 0){//need to swap
			int tempid = triPtIds[1];
			triPtIds[1] = triPtIds[2];
			triPtIds[2] = tempid;
		}		
	}
}

//Mainly for undo function
bool MouseInteractor::DrawTriangle(TagTriangle tagTri)
{
	//Update ID
	for(int i = 0; i < vectorTagPoints.size(); i++)
	{
		if(vectorTagPoints[i].seq == tagTri.seq1)
			tagTri.id1 = i;
		else if(vectorTagPoints[i].seq == tagTri.seq2)
			tagTri.id2 = i;
		else if(vectorTagPoints[i].seq == tagTri.seq3)
			tagTri.id3 = i;
	}
	//Update Edge
	vectorTagEdges[PairNumber(tagTri.id1, tagTri.id2)].numEdge++;
	vectorTagEdges[PairNumber(tagTri.id3, tagTri.id2)].numEdge++;
	vectorTagEdges[PairNumber(tagTri.id1, tagTri.id3)].numEdge++;

	vtkSmartPointer<vtkPoints> pts =
		vtkSmartPointer<vtkPoints>::New();
	pts->InsertNextPoint(tagTri.p1);
	pts->InsertNextPoint(tagTri.p2);
	pts->InsertNextPoint(tagTri.p3);	

	vtkSmartPointer<vtkTriangle> triangle =
		vtkSmartPointer<vtkTriangle>::New();
	triangle->GetPointIds()->SetId ( 0, 0 );
	triangle->GetPointIds()->SetId ( 1, 1 );
	triangle->GetPointIds()->SetId ( 2, 2 );

	vtkSmartPointer<vtkCellArray> triangles =
		vtkSmartPointer<vtkCellArray>::New();
	triangles->InsertNextCell ( triangle );

	// Create a polydata object
	vtkSmartPointer<vtkPolyData> trianglePolyData =
		vtkSmartPointer<vtkPolyData>::New();

	// Add the geometry and topology to the polydata
	trianglePolyData->SetPoints ( pts );
	trianglePolyData->SetPolys ( triangles );

	// Create mapper and actor
	vtkSmartPointer<vtkPolyDataMapper> mapper =
		vtkSmartPointer<vtkPolyDataMapper>::New();
#if VTK_MAJOR_VERSION <= 5
	mapper->SetInput(trianglePolyData);
	//mapper->SetInput(appendFilter->GetOutput());
#else
	mapper->SetInputData(trianglePolyData);
#endif
	vtkSmartPointer<vtkActor> actor =
		vtkSmartPointer<vtkActor>::New();
	actor->SetMapper(mapper);
	actor->GetProperty()->SetEdgeVisibility(true);
	actor->GetProperty()->SetEdgeColor(0.0,0.0,0.0);
	actor->GetProperty()->SetColor(triLabelColors[tagTri.index].red() / 255.0,
		triLabelColors[tagTri.index].green() / 255.0,
		triLabelColors[tagTri.index].blue() / 255.0);
	vtkSmartPointer<vtkProperty> backPro = 
		vtkSmartPointer<vtkProperty>::New();
	backPro->SetColor(backCol);
	actor->SetBackfaceProperty(backPro);

	tagTri.triActor = actor;
	vectorTagTriangles.push_back(tagTri);

	this->GetDefaultRenderer()->AddActor(actor);
	//Update actor
	for(int i = 0; i < vectorActions.size(); i++)
	{
		if(vectorActions[i].action == CREATETRI)
		{
			if(tagTri.seq1 == vectorActions[i].triangleInfo.seq1 &&
				tagTri.seq2 == vectorActions[i].triangleInfo.seq2 &&
				tagTri.seq2 == vectorActions[i].triangleInfo.seq2){
					vectorActions[i].triangleInfo.triActor = actor;
					break;
			}
		}
	}
	updateLabelTriNum();
	return true;
}

//For triangle creation
int MouseInteractor::DrawTriangle()
{
	int id1 = triPtIds[0], id2 = triPtIds[1], id3 = triPtIds[2];
	//Check if has duplicate point
	if (id1 == id2 || id1 == id3 || id2 == id3)
		return 0;
	
	int triId[3];
	triId[0] = triPtIds[0]; triId[1] = triPtIds[1]; triId[2] = triPtIds[2];
	//To see whether need to change the sequence of points
	CheckNormal(triId);
	//Update the sequence
	triPtIds[0] = triId[0]; triPtIds[1] = triId[1]; triPtIds[2] = triId[2];
	id1 = triPtIds[0], id2 = triPtIds[1], id3 = triPtIds[2];

	int edgeid1 = PairNumber(triPtIds[0], triPtIds[1]);
	int edgeid2 = PairNumber(triPtIds[1], triPtIds[2]);
	int edgeid3 = PairNumber(triPtIds[2], triPtIds[0]);
	std::cout<<"ID "<<id1<<" "<<id2<<" "<<id3<<std::endl;
	//Calculate the constrains for each edge
	int cons1 = ConstrainEdge(vectorTagInfo[vectorTagPoints[id1].comboBoxIndex].tagType, vectorTagInfo[vectorTagPoints[id2].comboBoxIndex].tagType);
	int cons2 = ConstrainEdge(vectorTagInfo[vectorTagPoints[id2].comboBoxIndex].tagType, vectorTagInfo[vectorTagPoints[id3].comboBoxIndex].tagType);
	int cons3 = ConstrainEdge(vectorTagInfo[vectorTagPoints[id1].comboBoxIndex].tagType, vectorTagInfo[vectorTagPoints[id3].comboBoxIndex].tagType);

	//Check if valid the constrain of each edge
	if(vectorTagEdges[edgeid1].numEdge >= cons1)
	{	
		int maxEdge = vectorTagEdges[edgeid1].numEdge;
		recordTriViolation = std::string("Edge number 1 already has ") + std::to_string(cons1) + std::string(" connexions and can only has ") + std::to_string(maxEdge) + std::string(" maximum.");
		return 0;
	}
	else if(vectorTagEdges[edgeid2].numEdge >= cons2)
	{
		int maxEdge = vectorTagEdges[edgeid2].numEdge;
		recordTriViolation = std::string("Edge number 2 already has ") + std::to_string(cons2) + std::string(" connexions and can only has ") + std::to_string(maxEdge) + std::string(" maximum.");
		return 0;
	}
	else if(vectorTagEdges[edgeid3].numEdge >= cons3)
	{
		int maxEdge = vectorTagEdges[edgeid3].numEdge;
		recordTriViolation = std::string("Edge number 3 already has ") + std::to_string(cons3) + std::string(" connexions and can only has ") + std::to_string(maxEdge) + std::string(" maximum.");
		return 0;
	}
	
	vectorTagEdges[edgeid1].numEdge++;
	vectorTagEdges[edgeid1].ptId1 = triPtIds[0]; vectorTagEdges[edgeid1].ptId2 = triPtIds[1];
	vectorTagEdges[edgeid2].numEdge++; 
	vectorTagEdges[edgeid2].ptId1 = triPtIds[1]; vectorTagEdges[edgeid2].ptId2 = triPtIds[2];
	vectorTagEdges[edgeid3].numEdge++; 
	vectorTagEdges[edgeid3].ptId1 = triPtIds[2]; vectorTagEdges[edgeid3].ptId2 = triPtIds[0];

	vtkSmartPointer<vtkPoints> pts =
		vtkSmartPointer<vtkPoints>::New();
	for(int i = 0; i < triPtIds.size(); i++){
		pts->InsertNextPoint(vectorTagPoints[triPtIds[i]].pos);
	}

	vtkSmartPointer<vtkTriangle> triangle =
		vtkSmartPointer<vtkTriangle>::New();
	triangle->GetPointIds()->SetId ( 0, 0 );
	triangle->GetPointIds()->SetId ( 1, 1 );
	triangle->GetPointIds()->SetId ( 2, 2 );

	vtkSmartPointer<vtkCellArray> triangles =
		vtkSmartPointer<vtkCellArray>::New();
	triangles->InsertNextCell ( triangle );

	// Create a polydata object
	vtkSmartPointer<vtkPolyData> trianglePolyData =
		vtkSmartPointer<vtkPolyData>::New();

	// Add the geometry and topology to the polydata
	trianglePolyData->SetPoints ( pts );
	trianglePolyData->SetPolys ( triangles );

	// Create mapper and actor
	vtkSmartPointer<vtkPolyDataMapper> mapper =
		vtkSmartPointer<vtkPolyDataMapper>::New();
#if VTK_MAJOR_VERSION <= 5
	mapper->SetInput(trianglePolyData);
	//mapper->SetInput(appendFilter->GetOutput());
#else
	mapper->SetInputData(trianglePolyData);
#endif
	vtkSmartPointer<vtkActor> actor =
		vtkSmartPointer<vtkActor>::New();
	actor->SetMapper(mapper);
	actor->GetProperty()->SetEdgeVisibility(true);
	actor->GetProperty()->SetEdgeColor(0.0,0.0,0.0);
	actor->GetProperty()->SetColor(triLabelColors[currentTriIndex].red() / 255.0,
		triLabelColors[currentTriIndex].green() / 255.0,
		triLabelColors[currentTriIndex].blue() / 255.0);
	vtkSmartPointer<vtkProperty> backPro = 
		vtkSmartPointer<vtkProperty>::New();
	backPro->SetColor(backCol);
	actor->SetBackfaceProperty(backPro);
	
	for(int i = 0; i < vectorTagTriangles.size(); i++)
	{
		if(vectorTagTriangles[i].centerPos[0] == actor->GetCenter()[0] &&
			vectorTagTriangles[i].centerPos[1] == actor->GetCenter()[1] &&
			vectorTagTriangles[i].centerPos[2] == actor->GetCenter()[2])
		{
			//illegal triangle, the triangle is already exist 
			recordTriViolation = "Triangle already exists";
			return 0;			
		}
	}

	//Store the new triangle
	TagTriangle tri;
	tri.triActor = actor;
	tri.p1[0] = vectorTagPoints[triPtIds[0]].pos[0]; tri.p1[1] = vectorTagPoints[triPtIds[0]].pos[1]; tri.p1[2] = vectorTagPoints[triPtIds[0]].pos[2];
	tri.p2[0] = vectorTagPoints[triPtIds[1]].pos[0]; tri.p2[1] = vectorTagPoints[triPtIds[1]].pos[1]; tri.p2[2] = vectorTagPoints[triPtIds[1]].pos[2];
	tri.p3[0] = vectorTagPoints[triPtIds[2]].pos[0]; tri.p3[1] = vectorTagPoints[triPtIds[2]].pos[1]; tri.p3[2] = vectorTagPoints[triPtIds[2]].pos[2];
	tri.id1 = triPtIds[0]; tri.id2 = triPtIds[1]; tri.id3 = triPtIds[2];
	tri.centerPos[0] = trianglePolyData->GetCenter()[0];
	tri.centerPos[1] = trianglePolyData->GetCenter()[1];
	tri.centerPos[2] = trianglePolyData->GetCenter()[2];
	tri.seq1 = vectorTagPoints[triPtIds[0]].seq;
	tri.seq2 = vectorTagPoints[triPtIds[1]].seq;
	tri.seq3 = vectorTagPoints[triPtIds[2]].seq;
	tri.index = currentTriIndex;
	vectorTagTriangles.push_back(tri);

	this->GetDefaultRenderer()->AddActor(actor);

	updateLabelTriNum();

	return 1;
}

TagTriangle MouseInteractor::DeleteTriangle(double* pos){

	vtkSmartPointer<vtkActor> pickedActor
		= vtkSmartPointer<vtkActor>::New();

	pickedActor = PickActorFromTriangle(pos);

	if(pickedActor != NULL)
	{
		pickedActor->GetProperty()->SetColor(0,0,0);
		for(int i = 0; i < vectorTagTriangles.size(); i++)
		{
			TagTriangle tri = vectorTagTriangles[i];
			if(pickedActor == tri.triActor)
			{
				if(vectorTagTriangles[i].id1 != -1 && vectorTagTriangles[i].id2 != -1 && vectorTagTriangles[i].id3 != -1)
				{
					vectorTagEdges[PairNumber(vectorTagTriangles[i].id1, vectorTagTriangles[i].id2)].numEdge--;
					vectorTagEdges[PairNumber(vectorTagTriangles[i].id3, vectorTagTriangles[i].id2)].numEdge--;
					vectorTagEdges[PairNumber(vectorTagTriangles[i].id1, vectorTagTriangles[i].id3)].numEdge--;
				}
				this->GetDefaultRenderer()->RemoveActor(pickedActor);
				//this->GetDefaultRenderer()->RemoveActor(triNormalActors[i]);
				vectorTagTriangles.erase(vectorTagTriangles.begin() + i);
				//triNormalActors.erase(triNormalActors.begin() + i);
				//break;
				updateLabelTriNum();
				return tri;
			}
		}
		
	}
	TagTriangle triNull;
	triNull.triActor = NULL;
	return triNull;
}

bool MouseInteractor::DeleteTriangle(TagTriangle tagTri)
{
	//Update id
	for(int i = 0; i < vectorTagPoints.size(); i++)
	{
		if(vectorTagPoints[i].seq == tagTri.seq1)
			tagTri.id1 = i;
		else if(vectorTagPoints[i].seq == tagTri.seq2)
			tagTri.id2 = i;
		else if(vectorTagPoints[i].seq == tagTri.seq3)
			tagTri.id3 = i;
	}

	int i;
	for(i = 0; i < vectorTagTriangles.size(); i++)
		if(vectorTagTriangles[i].triActor == tagTri.triActor)
			break;
	std::cout<<"DELETE TRI INDEX IS "<<i<<std::endl;
	if(i < vectorTagTriangles.size())
	{
		if(tagTri.id1 != -1 && tagTri.id2 != -1 && tagTri.id3 != -1)
		{
			vectorTagEdges[PairNumber(tagTri.id1, tagTri.id2)].numEdge--;
			vectorTagEdges[PairNumber(tagTri.id3, tagTri.id2)].numEdge--;
			vectorTagEdges[PairNumber(tagTri.id1, tagTri.id3)].numEdge--;
		}		
		this->GetDefaultRenderer()->RemoveActor(vectorTagTriangles[i].triActor);
		vectorTagTriangles.erase(vectorTagTriangles.begin() + i);
		updateLabelTriNum();
		return true;
	}
	return false;
}
	
vtkActor* MouseInteractor::PickActorFromPoints(double pos[3])
{
	vtkSmartPointer<vtkActor> pickedActor
		= vtkSmartPointer<vtkActor>::New();

	for(int i = 0; i < vectorTagPoints.size(); i++)
	{
		if(Distance(vectorTagPoints[i].pos, pos) < 1.0)
		{
			return vectorTagPoints[i].actor;
		}
	}
	return NULL;
}

vtkActor* MouseInteractor::PickActorFromTriangle(double pos[3])
{
	for(int i = 0; i < vectorTagTriangles.size(); i++)
	{
		vtkActor* triActor = vectorTagTriangles[i].triActor;

		vtkSmartPointer<vtkActorCollection> actorCollection 
			= vtkSmartPointer<vtkActorCollection>::New();
		triActor->GetActors(actorCollection);
		vtkTriangle* tri = vtkTriangle::SafeDownCast(actorCollection->GetItemAsObject(0));
		if(tri->PointInTriangle(pos, vectorTagTriangles[i].p1, vectorTagTriangles[i].p2, vectorTagTriangles[i].p3, 0.1f))
			return vectorTagTriangles[i].triActor;
	}
	return NULL;
}

bool MouseInteractor::FlipNormal(double* pos)
{
	vtkSmartPointer<vtkActor> pickedActor
		= vtkSmartPointer<vtkActor>::New();
	pickedActor = PickActorFromTriangle(pos);
	for(int i = 0; i < vectorTagTriangles.size(); i++)
	{
		if(vectorTagTriangles[i].triActor == pickedActor)
		{
			//flip the 2nd and 3rd vertices 
			int tempChange;
			tempChange = vectorTagTriangles[i].id2;
			vectorTagTriangles[i].id2 = vectorTagTriangles[i].id3;
			vectorTagTriangles[i].id3 = tempChange;

			tempChange = vectorTagTriangles[i].seq2;
			vectorTagTriangles[i].seq2 = vectorTagTriangles[i].seq3;
			vectorTagTriangles[i].seq3 = tempChange;

			double tempPos[3];
			tempPos[0] = vectorTagTriangles[i].p2[0]; tempPos[1] = vectorTagTriangles[i].p2[1]; tempPos[2] = vectorTagTriangles[i].p2[2]; 
			vectorTagTriangles[i].p2[0] = vectorTagTriangles[i].p3[0]; vectorTagTriangles[i].p2[1] = vectorTagTriangles[i].p3[1]; 
			vectorTagTriangles[i].p2[2] = vectorTagTriangles[i].p3[2];
			vectorTagTriangles[i].p3[0] = tempPos[0]; vectorTagTriangles[i].p3[1] = tempPos[1]; vectorTagTriangles[i].p3[2] = tempPos[2];

			//remove the original triangle
			this->GetDefaultRenderer()->RemoveActor(pickedActor);
			//this->GetDefaultRenderer()->RemoveActor(triNormalActors[i]);

			//create new triangle
			vtkSmartPointer<vtkPoints> pts =
				vtkSmartPointer<vtkPoints>::New();
			pts->InsertNextPoint(vectorTagTriangles[i].p1);
			pts->InsertNextPoint(vectorTagTriangles[i].p2);
			pts->InsertNextPoint(vectorTagTriangles[i].p3);

			vtkSmartPointer<vtkTriangle> triangle =
				vtkSmartPointer<vtkTriangle>::New();
			triangle->GetPointIds()->SetId ( 0, 0 );
			triangle->GetPointIds()->SetId ( 1, 1 );
			triangle->GetPointIds()->SetId ( 2, 2 );

			double *n = new double[3];
			triangle->ComputeNormal(vectorTagTriangles[i].p1, vectorTagTriangles[i].p2, vectorTagTriangles[i].p3, n);
			std::cout<<"triangle n "<<n[0]<<" "<<n[1]<<" "<<n[2]<<std::endl;

			vtkSmartPointer<vtkCellArray> triangles =
				vtkSmartPointer<vtkCellArray>::New();
			triangles->InsertNextCell ( triangle );

			// Create a polydata object
			vtkSmartPointer<vtkPolyData> trianglePolyData =
				vtkSmartPointer<vtkPolyData>::New();

			// Add the geometry and topology to the polydata
			trianglePolyData->SetPoints ( pts );
			trianglePolyData->SetPolys ( triangles );

			// Create mapper and actor
			vtkSmartPointer<vtkPolyDataMapper> mapper =
				vtkSmartPointer<vtkPolyDataMapper>::New();
#if VTK_MAJOR_VERSION <= 5
			mapper->SetInput(trianglePolyData);
#else
			mapper->SetInputData(trianglePolyData);
#endif
			vtkSmartPointer<vtkActor> actor =
				vtkSmartPointer<vtkActor>::New();
			actor->SetMapper(mapper);
			actor->GetProperty()->SetEdgeVisibility(true);
			actor->GetProperty()->SetEdgeColor(0.0,0.0,0.0);
			actor->GetProperty()->SetColor(vectorTagTriangles[i].triActor->GetProperty()->GetColor());
			vtkSmartPointer<vtkProperty> backPro = 
				vtkSmartPointer<vtkProperty>::New();
			backPro->SetColor(backCol);
			actor->SetBackfaceProperty(backPro);

			vectorTagTriangles[i].triActor = actor;

			this->GetDefaultRenderer()->AddActor(actor);

			return true;
		}
	}
	return false;
}

void MouseInteractor::AddDecimateEdge(int pointSeq)
{
	for(int i = 0; i < vectorTagTriangles.size(); i++)
	{
		bool track = false;
		if(vectorTagTriangles[i].seq1 == pointSeq){
			vectorTagTriangles[i].id1 = vectorTagPoints.size()-1;
			track = true;
		}
		else if(vectorTagTriangles[i].seq2 == pointSeq){
			vectorTagTriangles[i].id2 = vectorTagPoints.size()-1;
			track = true;
		}
		else if(vectorTagTriangles[i].seq3 == pointSeq){
			vectorTagTriangles[i].id3 = vectorTagPoints.size()-1;
			track = true;
		}

		if(track){
			//add edge info
			if(vectorTagTriangles[i].id1 != -1 && vectorTagTriangles[i].id2 != -1 && vectorTagTriangles[i].id3 != -1)
			{
				int id1 = vectorTagTriangles[i].id1; int id2 = vectorTagTriangles[i].id2; int id3 = vectorTagTriangles[i].id3;
				int edgeid1 = PairNumber(id1, id2);//id
				int edgeid2 = PairNumber(id2, id3);
				int edgeid3 = PairNumber(id3, id1);
				//std::cout<<"ID "<<id1<<" "<<id2<<" "<<id3<<std::endl;
				int cons1 = ConstrainEdge(vectorTagInfo[vectorTagPoints[id1].comboBoxIndex].tagType, vectorTagInfo[vectorTagPoints[id2].comboBoxIndex].tagType);
				int cons2 = ConstrainEdge(vectorTagInfo[vectorTagPoints[id2].comboBoxIndex].tagType, vectorTagInfo[vectorTagPoints[id3].comboBoxIndex].tagType);
				int cons3 = ConstrainEdge(vectorTagInfo[vectorTagPoints[id1].comboBoxIndex].tagType, vectorTagInfo[vectorTagPoints[id3].comboBoxIndex].tagType);

				if(vectorTagEdges[edgeid1].numEdge >= cons1){		
					return;
				}
				else if(vectorTagEdges[edgeid2].numEdge >= cons2){
					return;
				}
				else if(vectorTagEdges[edgeid3].numEdge >= cons3){
					return;
				}

				vectorTagEdges[edgeid1].numEdge++;
				vectorTagEdges[edgeid1].ptId1 = id1; vectorTagEdges[edgeid1].ptId2 = id2;
				vectorTagEdges[edgeid2].numEdge++; 
				vectorTagEdges[edgeid2].ptId1 = id2; vectorTagEdges[edgeid2].ptId2 = id3;
				vectorTagEdges[edgeid3].numEdge++; 
				vectorTagEdges[edgeid3].ptId1 = id3; vectorTagEdges[edgeid3].ptId2 = id1;
			}
		}
	}
}

//For Changing the triangle label and color
int MouseInteractor::ChangeTriLabel(double pos[3])
{
	vtkSmartPointer<vtkActor> pickedActor
		= vtkSmartPointer<vtkActor>::New();
	pickedActor = PickActorFromTriangle(pos);

	for(int i = 0; i < vectorTagTriangles.size(); i++)
	{
		if(pickedActor == vectorTagTriangles[i].triActor)
		{
			pickedActor->GetProperty()->SetColor(triLabelColors[currentTriIndex].red() / 255.0,
				triLabelColors[currentTriIndex].green() / 255.0,
				triLabelColors[currentTriIndex].blue() / 255.0);
			int temp = vectorTagTriangles[i].index;
			vectorTagTriangles[i].index = currentTriIndex;
			return temp;
		}
	}
	return -1;
}

//For Changing the point label and color
TagPoint MouseInteractor::ChangePtLabel(double pos[3])
{
    vtkSmartPointer<vtkActor> pickedActor
        = vtkSmartPointer<vtkActor>::New();
    pickedActor = PickActorFromPoints(pos);

    for(int i = 0; i < vectorTagPoints.size(); i++)
    {
        if(pickedActor == vectorTagPoints[i].actor)
        {
			//stock in a temp var the point info before modifications; mainly for Undo func
			TagPoint temp = vectorTagPoints[i];

			vectorTagPoints[i].actor->GetProperty()->SetColor(vectorTagInfo[Global::selectedTag].tagColor[0] / 255.0,
                vectorTagInfo[Global::selectedTag].tagColor[1] / 255.0,
                vectorTagInfo[Global::selectedTag].tagColor[2] / 255.0);

			vectorTagPoints[i].typeIndex = vectorTagInfo[Global::selectedTag].tagIndex;
			vectorTagPoints[i].comboBoxIndex = Global::selectedTag;
			int seq = vectorTagPoints[i].seq;
			labelData[seq] = vectorTagInfo[Global::selectedTag].tagIndex;

            return temp;
        }
    }
    TagPoint nullPts;
	nullPts.actor = NULL;
	return nullPts;
}

//Add point on skeleton
TagPoint MouseInteractor::AddPoint(double* pos)
{
	//See if there is any tag have been created
	if(vectorTagInfo.size() != 0)
	{
		//Find the first actor
		vtkSmartPointer<vtkActorCollection> actors = this->GetDefaultRenderer()->GetActors();
		vtkSmartPointer<vtkActor> actor0 =  static_cast<vtkActor *>(actors->GetItemAsObject(0));	
		vtkSmartPointer<vtkDataSet> vtkdata = actor0->GetMapper()->GetInputAsDataSet();
		vtkDoubleArray* radiusArray = (vtkDoubleArray*)vtkdata->GetPointData()->GetArray("Radius");
		double minDistance = DBL_MAX;
		double finalPos[3] = {};
		double pointRadius = 0.0;
		int pointSeq=0;

		if(!decimateMode){
			for(vtkIdType i = 0; i < vtkdata->GetNumberOfPoints(); i++){
				double p[3];
				vtkdata->GetPoint(i,p);
				double dist = Distance(pos, p);
				//Find the closet vertex
				if(dist < minDistance){
					minDistance = dist;
					pointSeq = i;
					finalPos[0] = p[0]; finalPos[1] = p[1]; finalPos[2] = p[2];
					pointRadius = radiusArray->GetValue(i);
				}
			}
		}
		else
		{
			for(int i = 0; i < vectorTagTriangles.size(); i++)
			{
				double dist = Distance(pos, vectorTagTriangles[i].p1);
				if(dist < minDistance){
					minDistance = dist;
					pointSeq = vectorTagTriangles[i].seq1;
					finalPos[0] = vectorTagTriangles[i].p1[0]; finalPos[1] = vectorTagTriangles[i].p1[1]; finalPos[2] = vectorTagTriangles[i].p1[2];
					pointRadius = radiusArray->GetValue(vectorTagTriangles[i].seq1);
				}

				dist = Distance(pos, vectorTagTriangles[i].p2);
				if(dist < minDistance){
					minDistance = dist;
					pointSeq = vectorTagTriangles[i].seq2;
					finalPos[0] = vectorTagTriangles[i].p2[0]; finalPos[1] = vectorTagTriangles[i].p2[1]; finalPos[2] = vectorTagTriangles[i].p2[2];
					pointRadius = radiusArray->GetValue(vectorTagTriangles[i].seq2);
				}

				dist = Distance(pos, vectorTagTriangles[i].p3);
				if(dist < minDistance){
					minDistance = dist;
					pointSeq = vectorTagTriangles[i].seq3;
					finalPos[0] = vectorTagTriangles[i].p3[0]; finalPos[1] = vectorTagTriangles[i].p3[1]; finalPos[2] = vectorTagTriangles[i].p3[2];
					pointRadius = radiusArray->GetValue(vectorTagTriangles[i].seq3);
				}
			}
		}

		std::cout << "Pick position (final position) is: "<< finalPos[0] << " " << finalPos[1]<< " " << finalPos[2] << std::endl;

		for(int i = 0; i < vectorTagPoints.size(); i++)
		{
			if(finalPos[0] == vectorTagPoints[i].pos[0] 
				&& finalPos[1] == vectorTagPoints[i].pos[1]
				&& finalPos[2] == vectorTagPoints[i].pos[2]){
					std::cout<<"duplicate point"<<std::endl;
					TagPoint nullPts;
					nullPts.actor = NULL;
					return nullPts;
				}

		}

		//Create a sphere
		vtkSmartPointer<vtkSphereSource> sphereSource =
			vtkSmartPointer<vtkSphereSource>::New();
		sphereSource->SetCenter(finalPos[0], finalPos[1], finalPos[2]);
		sphereSource->SetRadius(tagRadius);

		//Create a mapper and actor
		vtkSmartPointer<vtkPolyDataMapper> mapper =
			vtkSmartPointer<vtkPolyDataMapper>::New();
		mapper->SetInputConnection(sphereSource->GetOutputPort());

		TagInfo ti = vectorTagInfo[selectedTag];
		vtkSmartPointer<vtkActor> actor =
			vtkSmartPointer<vtkActor>::New();
		actor->SetMapper(mapper);
		actor->GetProperty()->SetColor(ti.tagColor[0] / 255.0, ti.tagColor[1] / 255.0, ti.tagColor[2] / 255.0);
						
		//Store actor in vectorTagPoints
		TagPoint actorT;
		actorT.actor = actor;
		actorT.typeIndex = ti.tagIndex;
		actorT.comboBoxIndex = selectedTag;
		actorT.radius = pointRadius;
		actorT.seq = pointSeq;
		actorT.pos[0] = finalPos[0]; actorT.pos[1] = finalPos[1]; actorT.pos[2] = finalPos[2];
		actorT.ptIndex = vectorTagPoints.size();
		vectorTagPoints.push_back(actorT);

		//Calculate the biggest number of edges possibility
		vectorTagEdges.resize(PairNumber(vectorTagPoints.size(), vectorTagPoints.size()));

		if(decimateMode)
			AddDecimateEdge(pointSeq);
							
		labelData[pointSeq] = ti.tagIndex;

		this->GetDefaultRenderer()->AddActor(actor);

		updateLabelPtNum();

		return actorT;
	}

	TagPoint nullPts;
	nullPts.actor = NULL;
	return nullPts;
}


//Mainly for undo function
bool MouseInteractor::AddPoint(TagPoint tagPt)
{
	//Create a sphere
	vtkSmartPointer<vtkSphereSource> sphereSource =
		vtkSmartPointer<vtkSphereSource>::New();
	sphereSource->SetCenter(tagPt.pos[0], tagPt.pos[1], tagPt.pos[2]);
	sphereSource->SetRadius(tagRadius);

	//Create a mapper and actor
	vtkSmartPointer<vtkPolyDataMapper> mapper =
		vtkSmartPointer<vtkPolyDataMapper>::New();
	mapper->SetInputConnection(sphereSource->GetOutputPort());

	TagInfo ti = vectorTagInfo[tagPt.comboBoxIndex];
	vtkSmartPointer<vtkActor> actor =
		vtkSmartPointer<vtkActor>::New();
	actor->SetMapper(mapper);
	actor->GetProperty()->SetColor(ti.tagColor[0] / 255.0, ti.tagColor[1] / 255.0, ti.tagColor[2] / 255.0);
	
	tagPt.actor = actor;
	vectorTagPoints.push_back(tagPt);

	if(decimateMode)
		AddDecimateEdge(tagPt.seq);
							
	labelData[tagPt.seq] = ti.tagIndex;

	this->GetDefaultRenderer()->AddActor(actor);

	updateLabelPtNum();
	
	for(int i = 0; i < vectorActions.size(); i++)
	{
		//Update the actor in VectorAction
		if(vectorActions[i].action == ADDPOINT)
		{
			if(vectorTagPoints[vectorTagPoints.size() - 1].seq == vectorActions[i].pointInfo.seq){
				vectorActions[i].pointInfo.actor = actor;
			}
		}
	}

	//Calculate the biggest number of edges possibility
	vectorTagEdges.resize(PairNumber(vectorTagPoints.size(), vectorTagPoints.size()));

	return true;
}


//Delete a point on skeleton
TagPoint MouseInteractor::DeletePoint(double* pos)
{
	vtkSmartPointer<vtkActor> pickedActor
		= vtkSmartPointer<vtkActor>::New();

	pickedActor = PickActorFromPoints(pos);

	//Erase from vectorTagPoints and color other points
	for(int i = 0; i < vectorTagPoints.size(); i++){
		TagPoint at = vectorTagPoints[i];
		vtkActor* tagActor = vectorTagPoints[i].actor;
        double* actorPos = new double[3];
        actorPos = at.actor->GetCenter();
        double deltaDis = Distance(pos,actorPos);
		//if(pickedActor != at.actor){							
		if(deltaDis > tagRadius){
			at.actor->GetProperty()->SetColor(vectorTagInfo[at.comboBoxIndex].tagColor[0] / 255.0,
				vectorTagInfo[at.comboBoxIndex].tagColor[1] / 255.0, 
				vectorTagInfo[at.comboBoxIndex].tagColor[2] / 255.0);
		}					
		else//Delete the point
		{
			labelData[vectorTagPoints[i].seq] = 0.0;							
			deleteEdge(i);
			
			TagPoint temp = vectorTagPoints[i];
			vectorTagPoints.erase(vectorTagPoints.begin() + i);

			updateLabelPtNum();
			updateLabelTriNum();
					
			vectorTagEdges.resize(PairNumber(vectorTagPoints.size(), vectorTagPoints.size()));
			this->GetDefaultRenderer()->RemoveActor(tagActor);

			return temp;
		}
	}
	TagPoint nullPts;
	nullPts.actor = NULL;
	return nullPts;
}


//Mainly for Undo function
bool MouseInteractor::DeletePoint(TagPoint tagpt)
{
	int i;
	for(i = 0; i < vectorTagPoints.size(); i++)
		if(vectorTagPoints[i].seq == tagpt.seq)
			break;
	std::cout<<"DELETE THE POINT INDEX IS "<<i<<std::endl;

	if(i < vectorTagPoints.size()){
		labelData[tagpt.seq] = 0.0;
		deleteEdge(i);

		vtkSmartPointer<vtkActor> tagActor = vectorTagPoints[i].actor;
		vectorTagPoints.erase(vectorTagPoints.begin() + i);

		updateLabelPtNum();
		updateLabelTriNum();

		vectorTagEdges.resize(PairNumber(vectorTagPoints.size(), vectorTagPoints.size()));
		this->GetDefaultRenderer()->RemoveActor(tagActor);

		return true;
	}
	return false;
}


//Pick Three points to create a triangle
int MouseInteractor::PickPointForTri(double* pos)
{
	//Deselect point
	for(int i = 0; i < triPtIds.size(); i ++)
	{
		double* actorPos = new double[3];
		actorPos = vectorTagPoints[triPtIds[i]].actor->GetCenter();
		double deltaDis = Distance(pos, actorPos);
		if(deltaDis <= tagRadius)
		{
			TagPoint at = vectorTagPoints[triPtIds[i]];
			vectorTagPoints[triPtIds[i]].actor->GetProperty()->SetColor(vectorTagInfo[at.comboBoxIndex].tagColor[0] / 255.0,
				vectorTagInfo[at.comboBoxIndex].tagColor[1] / 255.0, 
				vectorTagInfo[at.comboBoxIndex].tagColor[2] / 255.0);			
			DoAction(DESELECTPT, triPtIds[i]);
			triPtIds.erase(triPtIds.begin() + i);
			return 2;
		}
	}

	//Select Point
	for(int i = 0; i < vectorTagPoints.size(); i++)
	{
		TagPoint at = vectorTagPoints[i];
		double* actorPos = new double [3];
		actorPos = vectorTagPoints[i].actor->GetCenter();
		double deltaDis = Distance(pos, actorPos);
		if(deltaDis <= tagRadius){
			vectorTagPoints[i].actor->GetProperty()->SetColor(1,1,1);
			triPtIds.push_back(i);
			if(triPtIds.size() != 3)
				DoAction(PICKPTTRI);
			break;
		}
	}

	//Size of Selected point is equal 3, then create triangle
	if(triPtIds.size() == 3)
	{
		int reVal = DrawTriangle();			
		if(reVal != 0){
			DoAction(CREATETRI, vectorTagTriangles[vectorTagTriangles.size()-1]);
			//Auto select two other points if available
			SetNextTriPt();
		}
		else
			reset();
		this->Interactor->SetKeySym("");
		return reVal;
	}
	return 1;
}

void MouseInteractor::copyEdgeBtoA(int a, int b){
	vectorTagEdges[a].constrain = vectorTagEdges[b].constrain;
	vectorTagEdges[a].numEdge = vectorTagEdges[b].numEdge;
}

int MouseInteractor::deleteEdgeHelper(int id1, int id2, int seq)
{
	if(id1 < seq && id2 < seq)
		return PairNumber(id1, id2);
	else if(id1 < seq && id2 > seq)
		return PairNumber(id1, id2-1);
	else if(id1 > seq && id2 < seq)
		return PairNumber(id1-1, id2);
	else if(id1 > seq && id2 > seq)
		return PairNumber(id1 - 1 , id2 - 1);
}

int MouseInteractor::deleteEdgeHelper2(int id, int seq)
{
	if(id > seq)
		return id - 1;
	else
		return id;
}

//when delete a point, it may delete the edge. If not, then need to change the index of the edges
void MouseInteractor::deleteEdge(int seq)
{
	std::vector<TagEdge> temp;
	std::vector<int> clearEdgeId;
	for(int i = 0; i < vectorTagTriangles.size(); i++)
	{			
		int id1 = vectorTagTriangles[i].id1, id2 = vectorTagTriangles[i].id2, id3 = vectorTagTriangles[i].id3;
		int nori;			
		int n;
		TagEdge e1;
		//If the point is belongs to triangle
		if(id1 == seq || id2 == seq || id3 == seq)
		{
			int d1, d2;
			if(id1 == seq)
			{
				d1 = PairNumber(id1, id2);
				d2 = PairNumber(id1, id3);
				nori = PairNumber(id2,id3);
				n = deleteEdgeHelper(id2,id3,seq);
			}
			else if(id2 == seq)
			{
				d1 = PairNumber(id2, id3);
				d2 = PairNumber(id2, id1);
				nori = PairNumber(id1,id3);
				n = deleteEdgeHelper(id1,id3,seq);
			}
			else if(id3 == seq)
			{
				d1 = PairNumber(id3, id1);
				d2 = PairNumber(id3, id2);
				nori = PairNumber(id1,id2);
				n = deleteEdgeHelper(id1,id2,seq);
			}

			e1.seq = n;
			e1.constrain = vectorTagEdges[nori].constrain;
			e1.numEdge = --vectorTagEdges[nori].numEdge;
			e1.ptId1 = deleteEdgeHelper2(vectorTagEdges[nori].ptId1, seq);
			e1.ptId2 = deleteEdgeHelper2(vectorTagEdges[nori].ptId2, seq);
			temp.push_back(e1);
			clearEdgeId.push_back(nori);
			clearEdgeId.push_back(d1);
			clearEdgeId.push_back(d2);
			this->GetDefaultRenderer()->RemoveActor(vectorTagTriangles[i].triActor);
			vectorTagTriangles.erase(vectorTagTriangles.begin() + i);
			i--;
		}
		//If not belongs to triangle
		else
		{
			int minId = std::min(id1, std::min(id2, id3));
			int maxId = std::max(id1, std::max(id2, id3));
			//Need to change value if the point index is bigger than the maxId
			if(maxId > seq)
			{

				for(int j = 0; j < 3; j++)
				{
					if(j == 0){
						nori = PairNumber(id1,id2);
						n = deleteEdgeHelper(id1,id2,seq);
					}
					else if(j == 1){
						nori = PairNumber(id3,id2);
						n = deleteEdgeHelper(id3,id2,seq);
					}
					else if(j == 2){
						nori = PairNumber(id3,id1);
						n = deleteEdgeHelper(id3,id1,seq);
					}
					e1.seq = n;
					e1.constrain = vectorTagEdges[nori].constrain;
					e1.numEdge = vectorTagEdges[nori].numEdge;
					e1.ptId1 = deleteEdgeHelper2(vectorTagEdges[nori].ptId1, seq);
					e1.ptId2 = deleteEdgeHelper2(vectorTagEdges[nori].ptId2, seq);
					temp.push_back(e1);
					clearEdgeId.push_back(nori);
				}
			}
			vectorTagTriangles[i].id1 = deleteEdgeHelper2(id1,seq);
			vectorTagTriangles[i].id2 = deleteEdgeHelper2(id2,seq);
			vectorTagTriangles[i].id3 = deleteEdgeHelper2(id3,seq);
		}			
	}

	for (int i = 0; i < clearEdgeId.size(); i++)
	{
		int id = clearEdgeId[i];
		vectorTagEdges[id].constrain = vectorTagEdges[id].numEdge = vectorTagEdges[id].ptId1 = vectorTagEdges[id].ptId2 = 0;
	}

	for(int j = 0; j < temp.size(); j++)
	{
		int nu = temp[j].seq;
		vectorTagEdges[nu].constrain = temp[j].constrain;
		vectorTagEdges[nu].numEdge = temp[j].numEdge;
		vectorTagEdges[nu].ptId1 = temp[j].ptId1;
		vectorTagEdges[nu].ptId2 = temp[j].ptId2;
	}		
}

void MouseInteractor::reset()
{
	drawTriMode = false;
	for(int i = 0; i < triPtIds.size(); i++)
	{
		TagPoint at = vectorTagPoints[triPtIds[i]];
		vectorTagPoints[triPtIds[i]].actor->GetProperty()->SetColor(vectorTagInfo[at.comboBoxIndex].tagColor[0] / 255.0,
			vectorTagInfo[at.comboBoxIndex].tagColor[1] / 255.0, 
			vectorTagInfo[at.comboBoxIndex].tagColor[2] / 255.0);
	}
	triPtIds.resize(0);
	
	if(movePtIndex != -1)
	{
		TagPoint at = vectorTagPoints[movePtIndex];
		vectorTagPoints[movePtIndex].actor->GetProperty()->SetColor(vectorTagInfo[at.comboBoxIndex].tagColor[0] / 255.0,
			vectorTagInfo[at.comboBoxIndex].tagColor[1] / 255.0, 
			vectorTagInfo[at.comboBoxIndex].tagColor[2] / 255.0);
		movePtIndex = -1;
	}
	
}

void MouseInteractor::updateLabelPtNum()
{
	labelPtNumber->setText(QString::number(vectorTagPoints.size()));
}

void MouseInteractor::updateLabelTriNum()
{
	labelTriNumber->setText(QString::number(vectorTagTriangles.size()));
}
/*
void MouseInteractor::updateUndo()
{
	int size = vectorActions.size();
	if (size < 1)
		undoButton->setEnabled(false);
	else
		undoButton->setEnabled(true);
}

void MouseInteractor::updateRedo()
{
	int size = vectorRedoActions.size();
	if (size < 1)
		redoButton->setEnabled(false);
	else
		redoButton->setEnabled(true);
}
*/
//The old function, have been discarded
void MouseInteractor::DrawDelaunayTriangle(){
	if(vectorTagPoints.size() > 3){
		//clear triangle
		for(int i = 0; i < vectorTagTriangles.size(); i++)
		{
			this->GetDefaultRenderer()->RemoveActor(vectorTagTriangles[i].triActor);
		}
		vectorTagTriangles.clear();
		vectorTagEdges.clear();
		vectorTagEdges.resize(PairNumber(vectorTagPoints.size(), vectorTagPoints.size()));


		vtkSmartPointer<vtkPoints> pts =
			vtkSmartPointer<vtkPoints>::New();
		for(int i = 0; i < vectorTagPoints.size(); i++){
			pts->InsertNextPoint(vectorTagPoints[i].pos);
		}

		// Create a polydata to store everything in
		vtkSmartPointer<vtkPolyData> polyData = vtkSmartPointer<vtkPolyData>::New();
		polyData->SetPoints(pts);

        //Added lines // maybe wrong
        vtkSmartPointer<vtkPolyData> boundary =
                vtkSmartPointer<vtkPolyData>::New();
            boundary->SetPoints(polyData->GetPoints());

		vtkSmartPointer<vtkDelaunay2D> delaunay =
			vtkSmartPointer<vtkDelaunay2D>::New();

		#if VTK_MAJOR_VERSION <= 5
		delaunay->SetInput(polyData);
		#else
		delaunay->SetInputData(polyData);
		delaunay->SetSourceData(boundary);
		#endif
		delaunay->Update();

		vtkSmartPointer<vtkPolyDataMapper> meshMapper =
			vtkSmartPointer<vtkPolyDataMapper>::New();
		meshMapper->SetInputConnection(delaunay->GetOutputPort());

		vtkSmartPointer<vtkPolyData> triPolyData = vtkPolyData::SafeDownCast(meshMapper->GetInput());
		vtkIdType npts, *ptsTri;
		triPolyData->GetPolys()->InitTraversal();
		while(triPolyData->GetPolys()->GetNextCell(npts,ptsTri)) 
		{ 
			triPtIds.resize(3);
			triPtIds[0] = ptsTri[0]; triPtIds[1] = ptsTri[1]; triPtIds[2] = ptsTri[2];
			double dot1, dot2, dot3;
			double p1[3], p2[3], p3[3];
			p1[0] = vectorTagPoints[ptsTri[0]].pos[0]; p1[1] = vectorTagPoints[ptsTri[0]].pos[1]; p1[2] = vectorTagPoints[ptsTri[0]].pos[2];
			p2[0] = vectorTagPoints[ptsTri[1]].pos[0]; p2[1] = vectorTagPoints[ptsTri[1]].pos[1]; p2[2] = vectorTagPoints[ptsTri[1]].pos[2];
			p3[0] = vectorTagPoints[ptsTri[2]].pos[0]; p3[1] = vectorTagPoints[ptsTri[2]].pos[1]; p3[2] = vectorTagPoints[ptsTri[2]].pos[2];
			if(Distance(p1,p2)  > 25 || Distance(p2,p3) > 25 || Distance(p1,p3) > 25){
				//continue;
			}

			double e1[3], e2[3], e3[3];
			e1[0] = p2[0] - p1[0]; e1[1] = p2[1] - p1[1]; e1[2] = p2[2] - p1[2];
			e2[0] = p3[0] - p2[0]; e2[1] = p3[1] - p2[1]; e2[2] = p3[2] - p2[2];
			e3[0] = p1[0] - p3[0]; e3[1] = p1[1] - p3[1]; e3[2] = p1[2] - p3[2];
			vtkMath::Normalize(e1); vtkMath::Normalize(e2); vtkMath::Normalize(e3);
			dot1 = vtkMath::Dot(e1,e2);
			dot2 = vtkMath::Dot(e1,e3);
			dot3 = vtkMath::Dot(e2,e3);

			if(dot1 < -0.95 || dot2 < -0.95 || dot3 < -0.95){
				std::cout<<"pass"<<std::endl;
				continue;
			}
			DrawTriangle();
		} 
		triPtIds.clear();
	}
}

bool PairSort(std::pair<int,double> t1, std::pair<int,double> t2)
{
	return (t1.second < t2.second);
}

void MouseInteractor::AutoTriangulation()
{
	for(int i = 0; i < vectorTagPoints.size(); i++)
	{
		std::vector<std::pair<int, double> >idDis;
		for(int j = 0; j < vectorTagPoints.size(); j++)
		{
			std::vector<int>shortestId;
			if(i != j)
			{
				double dis = Distance(vectorTagPoints[i].pos, vectorTagPoints[j].pos);
				
				if(dis < 15)//threshold
				{
					std::pair<int,double> temp;
					temp.first = j;
					temp.second = dis;
					idDis.push_back(temp);
				}
			}
		}
		std::sort(idDis.begin(), idDis.end(), PairSort);
		if(idDis.size() >= 2)
		{
			triPtIds.clear();
			
			for(int j = 0; j < idDis.size(); j++){
				for(int k = j; k < idDis.size(); k ++){
					triPtIds.push_back(i);
					triPtIds.push_back(idDis[j].first);
					triPtIds.push_back(idDis[k].first);
					DrawTriangle();
					triPtIds.clear();
					std::cout<<"Create one"<<std::endl;
				}
			}			
		}		
	}
	
}

void MouseInteractor::DoAction(int action)
{
	TagAction ac;
	ac.action = action;
	vectorActions.push_back(ac);
	//updateUndo();
}

void MouseInteractor::DoAction(int action, double pos[3], int triIndex)
{
	TagAction ac;
	ac.action = action;
	ac.pos[0] = pos[0];
	ac.pos[1] = pos[1];
	ac.pos[2] = pos[2];
	ac.triIndex = triIndex;
	vectorActions.push_back(ac);
	//updateUndo();
}

void MouseInteractor::DoAction(int action, TagPoint pointInfo, int ptIndex)
{
	TagAction ac;
	ac.action = action;
	ac.pointInfo = pointInfo;
	ac.ptIndex = ptIndex;
	vectorActions.push_back(ac);
	//updateUndo();
}

void MouseInteractor::DoAction(int action, TagTriangle triangleInfo)
{
	TagAction ac;
	ac.action = action;
	ac.triangleInfo = triangleInfo;
	vectorActions.push_back(ac);
	//updateUndo();
}

void MouseInteractor::DoAction(int action, int ptIndex)
{
	TagAction ac;
	ac.action = action;
	ac.ptIndex = ptIndex;
	vectorActions.push_back(ac);
	//updateUndo();
}

void MouseInteractor::DoAction(int action, int ptIndex, int ptOldSeq)
{
	TagAction ac;
	ac.action = action;
	ac.ptIndex = ptIndex;
	ac.ptOldSeq = ptOldSeq;
	vectorActions.push_back(ac);
	//updateUndo();
}

void MouseInteractor::RedoAction(int action)
{
	TagAction ac;
	ac.action = action;
	vectorRedoActions.push_back(ac);
	//updateRedo();
}

void MouseInteractor::RedoAction(int action, double pos[3], int triIndex)
{
	TagAction ac;
	ac.action = action;
	ac.pos[0] = pos[0];
	ac.pos[1] = pos[1];
	ac.pos[2] = pos[2];
	ac.triIndex = triIndex;
	vectorRedoActions.push_back(ac);
	//updateRedo();
}

void MouseInteractor::RedoAction(int action, TagPoint pointInfo, int ptIndex)
{
	TagAction ac;
	ac.action = action;
	ac.pointInfo = pointInfo;
	ac.ptIndex = ptIndex;
	vectorRedoActions.push_back(ac);
	//updateRedo();
}

void MouseInteractor::RedoAction(int action, TagTriangle triangleInfo)
{
	TagAction ac;
	ac.action = action;
	ac.triangleInfo = triangleInfo;
	vectorRedoActions.push_back(ac);
	//updateRedo();
}

void MouseInteractor::RedoAction(int action, int ptIndex)
{
	TagAction ac;
	ac.action = action;
	ac.ptIndex = ptIndex;
	vectorRedoActions.push_back(ac);
	//updateRedo();
}

void MouseInteractor::RedoAction(int action, int ptIndex, int ptOldSeq)
{
	TagAction ac;
	ac.action = action;
	ac.ptIndex = ptIndex;
	ac.ptOldSeq = ptOldSeq;
	vectorRedoActions.push_back(ac);
	//updateRedo();
}

void MouseInteractor::UndoAction()
{
	int size = vectorActions.size() - 1;
	bool flag = false;
	if(size >= 0){		
		while(!flag){		
			int actionIndex = vectorActions[size].action;
			//std::cout<<"UNDO ACTION IS "<<actionIndex<<std::endl;
			//cout<<"UNDO SIZE IS "<<size;
			if(actionIndex == ADDPOINT)
			{
				reset();
                flag = DeletePoint(vectorActions[size].pointInfo);
				vectorActions[size].action = DELETEPOINT;
				vectorRedoActions.push_back(vectorActions[size]);
				vectorActions.erase(vectorActions.end() - 1);
			}
			else if(actionIndex == DELETEPOINT)
			{
				reset();
				flag = AddPoint(vectorActions[size].pointInfo);
				vectorActions[size].action = ADDPOINT;
				vectorRedoActions.push_back(vectorActions[size]);
				vectorActions.erase(vectorActions.end() - 1);
			}
			else if(actionIndex == FLIPNORMAL)
			{
				reset();
				flag = FlipNormal(vectorActions[size].pos);
				vectorRedoActions.push_back(vectorActions[size]);
				vectorActions.erase(vectorActions.end() - 1);
			}
			else if(actionIndex == PICKPTTRI)
			{
				if(triPtIds.size() > 0){
					int index = triPtIds[triPtIds.size() - 1];
					TagPoint at = vectorTagPoints[index];
					vectorTagPoints[index].actor->GetProperty()->SetColor(vectorTagInfo[at.comboBoxIndex].tagColor[0] / 255.0,
						vectorTagInfo[at.comboBoxIndex].tagColor[1] / 255.0, 
						vectorTagInfo[at.comboBoxIndex].tagColor[2] / 255.0);
					triPtIds.erase(triPtIds.begin() + triPtIds.size()-1);
					flag = true;
				}
				vectorActions[size].action = DESELECTPT;
				vectorRedoActions.push_back(vectorActions[size]);
				vectorActions.erase(vectorActions.end() - 1);
			}
			else if(actionIndex == DESELECTPT)
			{
				int index = vectorActions[size].ptIndex;
				if(index < vectorTagPoints.size())
				{
					vectorTagPoints[index].actor->GetProperty()->SetColor(1,1,1);
					triPtIds.push_back(index);
					flag = true;
				}
				vectorActions[size].action = PICKPTTRI;
				vectorRedoActions.push_back(vectorActions[size]);
				vectorActions.erase(vectorActions.end() - 1);
			}
			else if(actionIndex == CREATETRI)
			{
				reset();
				flag = DeleteTriangle(vectorActions[size].triangleInfo);
				vectorActions[size].action = DELETETRI;
				vectorRedoActions.push_back(vectorActions[size]);
				vectorActions.erase(vectorActions.end() - 1);
				int sizeT = vectorActions.size() -1;
				for(int i = vectorActions.size() - 1; i > sizeT - 3; i--){
					if(vectorActions[i].action == PICKPTTRI)
						vectorActions.erase(vectorActions.end() - 1);
					else
						break;
				}
			}
			else if(actionIndex == DELETETRI)
			{
				reset();
				flag = DrawTriangle(vectorActions[size].triangleInfo);
				vectorActions[size].action = CREATETRI;
				vectorRedoActions.push_back(vectorActions[size]);
				vectorActions.erase(vectorActions.end() - 1);
			}
			else if(actionIndex == CHANGETRILABEL)
			{
				reset();
				vtkSmartPointer<vtkActor> actor = PickActorFromTriangle(vectorActions[size].pos);
				if(actor != NULL)
				{
					for(int i = 0; i < vectorTagTriangles.size(); i++)
					{
						if(actor == vectorTagTriangles[i].triActor)
						{
							int triIndex = vectorActions[size].triIndex;

							double pos[3];
							pos[0] = vectorActions[size].pos[0];
							pos[1] = vectorActions[size].pos[1];
							pos[2] = vectorActions[size].pos[2];
							int tempIndex = vectorTagTriangles[i].index;
							RedoAction(CHANGETRILABEL, pos, tempIndex);

							actor->GetProperty()->SetColor(triLabelColors[triIndex].red() / 255.0,
															triLabelColors[triIndex].green() / 255.0,
															triLabelColors[triIndex].blue() / 255.0);
							vectorTagTriangles[i].index = vectorActions[size].triIndex;
						}
					}
					std::cout << "LABEL CHANGED" << std::endl;
					flag = true;					
				}
				//vectorRedoActions.push_back(vectorActions[size]);
				vectorActions.erase(vectorActions.end() - 1);
			}
			else if (actionIndex == EDITTAGPT)
			{
				reset();
				vtkSmartPointer<vtkActor> actor = PickActorFromPoints(vectorActions[size].pointInfo.pos);
				if (actor != NULL)
				{
					for (int i = 0; i < vectorTagPoints.size(); i++)
					{
						if (actor == vectorTagPoints[i].actor)
						{
							int tagIndex = vectorActions[size].ptIndex;
							int comboIndex = vectorActions[size].pointInfo.comboBoxIndex;

							//Stock info before modif for Redo func
							TagPoint temp = vectorTagPoints[i];
							RedoAction(EDITTAGPT, temp, temp.typeIndex);

							vectorTagPoints[i].actor->GetProperty()->SetColor(vectorTagInfo[comboIndex].tagColor[0] / 255.0,
								vectorTagInfo[comboIndex].tagColor[1] / 255.0,
								vectorTagInfo[comboIndex].tagColor[2] / 255.0);

							vectorTagPoints[i].typeIndex = tagIndex;
							vectorTagPoints[i].comboBoxIndex = comboIndex;
							int seq = vectorTagPoints[i].seq;
							labelData[seq] = tagIndex;
						}
					}
					std::cout << "TAG CHANGED" << std::endl;
					flag = true;
				}
				//vectorRedoActions.push_back(vectorActions[size]);
				vectorActions.erase(vectorActions.end() - 1);
			}
			else if(actionIndex == MOVEPT)
			{
				MovePoint(vectorActions[size].ptIndex, vectorActions[size].ptOldSeq);
				movePtIndex = -1;
				vectorRedoActions.push_back(vectorActions[size]);
				vectorActions.erase(vectorActions.end() - 1);
				flag = true;
			}
			this->GetDefaultRenderer()->GetRenderWindow()->Render();
			size = vectorActions.size() - 1;
			//updateUndo();
			//updateRedo();
		}
	}
}

void MouseInteractor::RedoAction()
{
	int size = vectorRedoActions.size() - 1;
	bool flag = false;
	if (size >= 0) {
		while (!flag) {
			int actionIndex = vectorRedoActions[size].action;
			//std::cout<<"UNDO ACTION IS "<<actionIndex<<std::endl;
			//cout<<"UNDO SIZE IS "<<size;
			if (actionIndex == ADDPOINT)
			{
				reset();
				flag = DeletePoint(vectorRedoActions[size].pointInfo);
				vectorRedoActions[size].action = DELETEPOINT;
				vectorActions.push_back(vectorRedoActions[size]);
				vectorRedoActions.erase(vectorRedoActions.end() - 1);
			}
			else if (actionIndex == DELETEPOINT)
			{
				reset();
				flag = AddPoint(vectorRedoActions[size].pointInfo);
				vectorRedoActions[size].action = ADDPOINT;
				vectorActions.push_back(vectorRedoActions[size]);
				vectorRedoActions.erase(vectorRedoActions.end() - 1);
			}
			else if (actionIndex == FLIPNORMAL)
			{
				reset();
				flag = FlipNormal(vectorRedoActions[size].pos);
				vectorActions.push_back(vectorRedoActions[size]);
				vectorRedoActions.erase(vectorRedoActions.end() - 1);
			}
			else if (actionIndex == PICKPTTRI)
			{
				if (triPtIds.size() > 0) {
					int index = triPtIds[triPtIds.size() - 1];
					TagPoint at = vectorTagPoints[index];
					vectorTagPoints[index].actor->GetProperty()->SetColor(vectorTagInfo[at.comboBoxIndex].tagColor[0] / 255.0,
						vectorTagInfo[at.comboBoxIndex].tagColor[1] / 255.0,
						vectorTagInfo[at.comboBoxIndex].tagColor[2] / 255.0);
					triPtIds.erase(triPtIds.begin() + triPtIds.size() - 1);
					flag = true;
				}
				vectorRedoActions[size].action = DESELECTPT;
				vectorActions.push_back(vectorRedoActions[size]);
				vectorRedoActions.erase(vectorRedoActions.end() - 1);
			}
			else if (actionIndex == DESELECTPT)
			{
				int index = vectorRedoActions[size].ptIndex;
				if (index < vectorTagPoints.size())
				{
					vectorTagPoints[index].actor->GetProperty()->SetColor(1, 1, 1);
					triPtIds.push_back(index);
					flag = true;
				}
				vectorRedoActions[size].action = PICKPTTRI;
				vectorActions.push_back(vectorRedoActions[size]);
				vectorRedoActions.erase(vectorRedoActions.end() - 1);
			}
			else if (actionIndex == CREATETRI)
			{
				reset();
				flag = DeleteTriangle(vectorRedoActions[size].triangleInfo);
				vectorRedoActions[size].action = DELETETRI;
				vectorActions.push_back(vectorRedoActions[size]);
				vectorRedoActions.erase(vectorRedoActions.end() - 1);
				int sizeT = vectorRedoActions.size() - 1;
				for (int i = vectorRedoActions.size() - 1; i > sizeT - 3; i--) {
					if (vectorRedoActions[i].action == PICKPTTRI)
						vectorRedoActions.erase(vectorRedoActions.end() - 1);
					else
						break;
				}
			}
			else if (actionIndex == DELETETRI)
			{
				reset();
				flag = DrawTriangle(vectorRedoActions[size].triangleInfo);
				vectorRedoActions[size].action = CREATETRI;
				vectorActions.push_back(vectorRedoActions[size]);
				vectorRedoActions.erase(vectorRedoActions.end() - 1);
			}
			else if (actionIndex == CHANGETRILABEL)
			{
				reset();
				vtkSmartPointer<vtkActor> actor = PickActorFromTriangle(vectorRedoActions[size].pos);
				if (actor != NULL)
				{
					for (int i = 0; i < vectorTagTriangles.size(); i++)
					{
						if (actor == vectorTagTriangles[i].triActor)
						{
							int triIndex = vectorRedoActions[size].triIndex;

							double pos[3];
							pos[0] = vectorRedoActions[size].pos[0];
							pos[1] = vectorRedoActions[size].pos[1];
							pos[2] = vectorRedoActions[size].pos[2];
							int tempIndex = vectorTagTriangles[i].index;
							DoAction(CHANGETRILABEL, pos, tempIndex);

							actor->GetProperty()->SetColor(triLabelColors[triIndex].red() / 255.0,
								triLabelColors[triIndex].green() / 255.0,
								triLabelColors[triIndex].blue() / 255.0);
							vectorTagTriangles[i].index = triIndex;
						}
					}
					std::cout << "LABEL CHANGED" << std::endl;
					flag = true;
				}
				//vectorActions.push_back(vectorRedoActions[size]);
				vectorRedoActions.erase(vectorRedoActions.end() - 1);
			}
			else if (actionIndex == EDITTAGPT)
			{
				reset();
				vtkSmartPointer<vtkActor> actor = PickActorFromPoints(vectorRedoActions[size].pointInfo.pos);
				if (actor != NULL)
				{
					for (int i = 0; i < vectorTagPoints.size(); i++)
					{
						if (actor == vectorTagPoints[i].actor)
						{
							int tagIndex = vectorRedoActions[size].ptIndex;
							int comboIndex = vectorRedoActions[size].pointInfo.comboBoxIndex;

							TagPoint temp = vectorTagPoints[i];
							DoAction(EDITTAGPT, temp, temp.typeIndex);

							vectorTagPoints[i].actor->GetProperty()->SetColor(vectorTagInfo[comboIndex].tagColor[0] / 255.0,
								vectorTagInfo[comboIndex].tagColor[1] / 255.0,
								vectorTagInfo[comboIndex].tagColor[2] / 255.0);

							vectorTagPoints[i].typeIndex = tagIndex;
							vectorTagPoints[i].comboBoxIndex = comboIndex;
							int seq = vectorTagPoints[i].seq;
							labelData[seq] = tagIndex;
						}
					}
					std::cout << "TAG CHANGED" << std::endl;
					flag = true;
				}
				//vectorActions.push_back(vectorRedoActions[size]);
				vectorRedoActions.erase(vectorRedoActions.end() - 1);
			}
			else if (actionIndex == MOVEPT)
			{
				MovePoint(vectorRedoActions[size].ptIndex, vectorRedoActions[size].ptOldSeq);
				movePtIndex = -1;
				vectorActions.push_back(vectorRedoActions[size]);
				vectorRedoActions.erase(vectorRedoActions.end() - 1);
				flag = true;
			}
			this->GetDefaultRenderer()->GetRenderWindow()->Render();
			size = vectorRedoActions.size() - 1;
			//updateUndo();
			//updateRedo();
		}
	}
}

bool MouseInteractor::isValidEdge(int id1, int id2)
{
	int edgeid = PairNumber(id1, id2);//id
	int cons = ConstrainEdge(vectorTagInfo[vectorTagPoints[id1].comboBoxIndex].tagType, vectorTagInfo[vectorTagPoints[id2].comboBoxIndex].tagType);

	if(vectorTagEdges[edgeid].numEdge >= cons){		
		return false;
	}
	return true;
}

void MouseInteractor::SetNextTriPtHelper(int id1, int id2)
{
	reset();
	vectorTagPoints[id1].actor->GetProperty()->SetColor(1,1,1);
	vectorTagPoints[id2].actor->GetProperty()->SetColor(1,1,1);
	triPtIds.push_back(id1);
	triPtIds.push_back(id2);
	drawTriMode = true;
}

//Find the next two point for triangle creation
void MouseInteractor::SetNextTriPt()
{
	if(isValidEdge(triPtIds[1], triPtIds[2]))
	{
		SetNextTriPtHelper(triPtIds[1], triPtIds[2]);	
	}
	else if(isValidEdge(triPtIds[0], triPtIds[1]))
	{
		SetNextTriPtHelper(triPtIds[0], triPtIds[1]);	
	}
	else if(isValidEdge(triPtIds[0], triPtIds[2]))
	{
		SetNextTriPtHelper(triPtIds[0], triPtIds[2]);	
	}
	else
		reset();
}

vtkStandardNewMacro(MouseInteractor);
