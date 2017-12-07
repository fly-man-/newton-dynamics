/* Copyright (c) <2003-2016> <Newton Game Dynamics>
* 
* This software is provided 'as-is', without any express or implied
* warranty. In no event will the authors be held liable for any damages
* arising from the use of this software.
* 
* Permission is granted to anyone to use this software for any purpose,
* including commercial applications, and to alter it and redistribute it
* freely
*/

#include "toolbox_stdafx.h"
#include "SkyBox.h"
#include "DemoMesh.h"
#include "DemoCamera.h"
#include "DebugDisplay.h"
#include "PhysicsUtils.h"
#include "TargaToOpenGl.h"
#include "DemoEntityManager.h"
#include "dCustomBallAndSocket.h"
#include "HeightFieldPrimitive.h"


class dSixAxisController: public dCustomControllerBase
{
	public:
	class dKukaServoMotor1: public dCustomHinge
	{
		public:
		dKukaServoMotor1(const dMatrix& pinAndPivotFrame, NewtonBody* const child, NewtonBody* const parent)
			:dCustomHinge(pinAndPivotFrame, child, parent)
			,m_torque(1000.0f)
		{
			EnableLimits(false);
		}

		void SubmitConstraintsFreeDof(dFloat timestep, const dMatrix& matrix0, const dMatrix& matrix1)
		{
			NewtonUserJointAddAngularRow(m_joint, 0.0f, &matrix1.m_front[0]);
			dFloat accel = NewtonUserJointGetRowInverseDynamicsAcceleration(m_joint);
			NewtonUserJointSetRowAcceleration(m_joint, accel);
			NewtonUserJointSetRowMinimumFriction(m_joint, -m_torque);
			NewtonUserJointSetRowMaximumFriction(m_joint, m_torque);
		}

		void Debug(dDebugDisplay* const debugDisplay) const
		{
		}

		dFloat m_torque;
	};

	class dKukaServoMotor2: public dCustomUniversal
	{
		public:
		dKukaServoMotor2(const dMatrix& pinAndPivotFrame, NewtonBody* const child, NewtonBody* const parent)
			:dCustomUniversal(pinAndPivotFrame, child, parent)
			,m_torque(1000.0f)
		{
			EnableLimit_0(false);
			EnableLimit_1(false);
		}

		void Debug(dDebugDisplay* const debugDisplay) const
		{
		}

		void SubmitConstraints(dFloat timestep, int threadIndex)
		{
			dMatrix matrix0;
			dMatrix matrix1;
			dFloat accel;

			dCustomUniversal::SubmitConstraints(timestep, threadIndex);

			// calculate the position of the pivot point and the Jacobian direction vectors, in global space. 
			CalculateGlobalMatrix(matrix0, matrix1);

			NewtonUserJointAddAngularRow(m_joint, 0.0f, &matrix0.m_front[0]);
			accel = NewtonUserJointGetRowInverseDynamicsAcceleration(m_joint);
			NewtonUserJointSetRowAcceleration(m_joint, accel);
			NewtonUserJointSetRowMinimumFriction(m_joint, -m_torque);
			NewtonUserJointSetRowMaximumFriction(m_joint, m_torque);

			NewtonUserJointAddAngularRow(m_joint, 0.0f, &matrix1.m_up[0]);
			accel = NewtonUserJointGetRowInverseDynamicsAcceleration(m_joint);
			NewtonUserJointSetRowAcceleration(m_joint, accel);
			NewtonUserJointSetRowMinimumFriction(m_joint, -m_torque);
			NewtonUserJointSetRowMaximumFriction(m_joint, m_torque);
		}

		dFloat m_torque;
	};


	class dKukaEndEffector: public dCustomKinematicController
	{
		public:
		dKukaEndEffector(NewtonInverseDynamics* const invDynSolver, void* const invDynNode, const dMatrix& attachmentPointInGlobalSpace, const dVector& origin)
			:dCustomKinematicController(invDynSolver, invDynNode, attachmentPointInGlobalSpace)
			,m_origin(origin)
		{
			m_x0 = attachmentPointInGlobalSpace.m_posit.m_z - m_origin.m_z;
			m_y0 = attachmentPointInGlobalSpace.m_posit.m_y - m_origin.m_y;
			SetPickMode(0);
			SetMaxLinearFriction (1000.0f);
		}

		dKukaEndEffector(NewtonBody* const body, const dMatrix& attachmentPointInGlobalSpace, const dVector& origin)
			:dCustomKinematicController(body, attachmentPointInGlobalSpace)
			,m_origin(origin)
		{
			m_x0 = attachmentPointInGlobalSpace.m_posit.m_z - m_origin.m_z;
			m_y0 = attachmentPointInGlobalSpace.m_posit.m_y - m_origin.m_y;
			SetPickMode(0);
			SetMaxLinearFriction(1000.0f);
		}


		~dKukaEndEffector()
		{
		}

		void Debug(dDebugDisplay* const debugDisplay) const
		{
			dCustomKinematicController::Debug(debugDisplay);
		}

		void SetTarget (dFloat x, dFloat y, dFloat azymuth)
		{
			x += 0.5f;
			dMatrix matrix(dYawMatrix(azymuth));
			dVector target(0.0f, m_y0 + y, m_x0 + x);
			matrix.m_posit = m_origin + matrix.RotateVector(target);

			m_targetMatrix = matrix;
			SetTargetPosit(matrix.m_posit);
		}

		void SubmitConstraints(dFloat timestep, int threadIndex)
		{
			dMatrix matrix0;
			dMatrix matrix1;
			dVector veloc(0.0f);
			dVector omega(0.0f);
			dVector com(0.0f);
			dVector pointVeloc(0.0f);

			dFloat invTimestep = 1.0f / timestep;

			CalculateGlobalMatrix(matrix0, matrix1);

			// calculate the position of the pivot point and the Jacobian direction vectors, in global space. 
			dVector targetPosit(m_targetMatrix.m_posit);
			dVector relPosit(targetPosit - matrix0.m_posit);
			
			dFloat mag2 = relPosit.DotProduct3(relPosit);
			if (mag2 > 1.0e-4f) {
//				dMatrix dirMatrix(dGrammSchmidt(relPosit));
				const dMatrix& dirMatrix = m_targetMatrix;

				NewtonBodyGetPointVelocity(m_body0, &targetPosit[0], &pointVeloc[0]);

				
				dFloat dist;
				dFloat speed;
				dFloat relSpeed;
				dFloat relAccel;
				dFloat damp = 0.3f;

				// Restrict the movement on the pivot point along all tree orthonormal direction
				speed = pointVeloc.DotProduct3(dirMatrix.m_front);
				dist = relPosit.DotProduct3(dirMatrix.m_front) * damp;
				relSpeed = dist * invTimestep - speed;
				relAccel = relSpeed * invTimestep;
				NewtonUserJointAddLinearRow(m_joint, &matrix0.m_posit[0], &matrix0.m_posit[0], &dirMatrix.m_front[0]);
				NewtonUserJointSetRowAcceleration(m_joint, relAccel);
				NewtonUserJointSetRowMinimumFriction(m_joint, -m_maxLinearFriction);
				NewtonUserJointSetRowMaximumFriction(m_joint, m_maxLinearFriction);

				speed = pointVeloc.DotProduct3(dirMatrix.m_up);
				dist = relPosit.DotProduct3(dirMatrix.m_up) * damp;
				relSpeed = dist * invTimestep - speed;
				relAccel = relSpeed * invTimestep;
				NewtonUserJointAddLinearRow(m_joint, &matrix0.m_posit[0], &matrix0.m_posit[0], &dirMatrix.m_up[0]);
				NewtonUserJointSetRowAcceleration(m_joint, relAccel);
				NewtonUserJointSetRowMinimumFriction(m_joint, -m_maxLinearFriction);
				NewtonUserJointSetRowMaximumFriction(m_joint, m_maxLinearFriction);


				speed = pointVeloc.DotProduct3(dirMatrix.m_right);
				dist = relPosit.DotProduct3(dirMatrix.m_right) * damp;
				relSpeed = dist * invTimestep - speed;
				relAccel = relSpeed * invTimestep;
				NewtonUserJointAddLinearRow(m_joint, &matrix0.m_posit[0], &matrix0.m_posit[0], &dirMatrix.m_right[0]);
				NewtonUserJointSetRowAcceleration(m_joint, relAccel);
				NewtonUserJointSetRowMinimumFriction(m_joint, -m_maxLinearFriction);
				NewtonUserJointSetRowMaximumFriction(m_joint, m_maxLinearFriction);

			} else {
				// Restrict the movement on the pivot point along all tree orthonormal direction
				NewtonUserJointAddLinearRow(m_joint, &matrix0.m_posit[0], &targetPosit[0], &matrix0.m_front[0]);
				NewtonUserJointSetRowMinimumFriction(m_joint, -m_maxLinearFriction);
				NewtonUserJointSetRowMaximumFriction(m_joint, m_maxLinearFriction);

				NewtonUserJointAddLinearRow(m_joint, &matrix0.m_posit[0], &targetPosit[0], &matrix0.m_up[0]);
				NewtonUserJointSetRowMinimumFriction(m_joint, -m_maxLinearFriction);
				NewtonUserJointSetRowMaximumFriction(m_joint, m_maxLinearFriction);

				NewtonUserJointAddLinearRow(m_joint, &matrix0.m_posit[0], &targetPosit[0], &matrix0.m_right[0]);
				NewtonUserJointSetRowMinimumFriction(m_joint, -m_maxLinearFriction);
				NewtonUserJointSetRowMaximumFriction(m_joint, m_maxLinearFriction);
			}
		}

		dMatrix m_targetMatrix;
		dVector m_origin;
		dFloat m_x0;
		dFloat m_y0;
	};

	dSixAxisController()
		:m_effector(NULL)
		,m_kinematicSolver(NULL)
		,m_azimuth(0.0f)
		,m_posit_x(0.0f)
		,m_posit_y(0.0f)
	{
	}

	void Init()
	{
		m_kinematicSolver = NewtonCreateInverseDynamics(GetManager()->GetWorld());
	}

	void SetTarget (dFloat x, dFloat y, dFloat azymuth)
	{
		m_posit_x = x;
		m_posit_y = y;
		m_azimuth = azymuth;
	}

#if 0
	void MakeKukaRobot_IK(DemoEntityManager* const scene, const dVector& origin)
	{
		dMatrix location(dRollMatrix(90.0f * 3.141592f / 180.0f));
		location.m_posit = origin;
		location.m_posit.m_y += 0.125f * 0.5f;

		// add Robot Base
		NewtonBody* const parentBody = CreateCylinder(scene, location, 0.35f, 0.125f);
		dMatrix parentMatrix(dGrammSchmidt(dVector(0.0f, 1.0f, 0.0f)));
		parentMatrix.m_posit = location.m_posit;
		dCustomHinge* const fixHinge = new dCustomHinge(parentMatrix, parentBody, NULL);
		fixHinge->EnableLimits(true);
		fixHinge->SetLimits(0.0f, 0.0f);
		void* const rootNode = NewtonInverseDynamicsAddRoot(m_kinematicSolver, parentBody);

		// add Robot rotating platform
		dMatrix baseMatrix(dGetIdentityMatrix());
		baseMatrix.m_posit = location.m_posit;
		baseMatrix.m_posit.m_y += 0.125f * 0.5f + 0.11f;
		baseMatrix.m_posit.m_z += 0.125f * 0.5f;
		NewtonBody* const base = CreateBox(scene, baseMatrix, dVector(0.125f, 0.2f, 0.25f));
		dMatrix baseSpin(dGrammSchmidt(dVector(0.0f, 1.0f, 0.0f)));
		baseSpin.m_posit = location.m_posit;
		dKukaServoMotor1* const baseHinge = new dKukaServoMotor1(baseSpin, base, parentBody);
		void* const baseHingeNode = NewtonInverseDynamicsAddChildNode(m_kinematicSolver, rootNode, baseHinge->GetJoint());

		// add Robot Arm
		dMatrix armMatrix0(dPitchMatrix(45.0f * 3.141592f / 180.0f));
		armMatrix0.m_posit = baseMatrix.m_posit;
		armMatrix0.m_posit.m_y += 0.30f;
		armMatrix0.m_posit.m_x += 0.09f;
		armMatrix0.m_posit.m_z -= 0.125f;
		NewtonBody* const armBody0 = CreateBox(scene, armMatrix0, dVector(0.05f, 0.1f, 0.75f));
		dMatrix armHingeMatrix0(dGrammSchmidt(dVector(1.0f, 0.0f, 0.0f)));
		armHingeMatrix0.m_posit = armMatrix0.m_posit + armMatrix0.RotateVector(dVector(0.0f, 0.0f, 0.3f));
		dKukaServoMotor1* const armJoint0 = new dKukaServoMotor1(armHingeMatrix0, armBody0, base);
		void* const armJointNode0 = NewtonInverseDynamicsAddChildNode(m_kinematicSolver, baseHingeNode, armJoint0->GetJoint());

		dMatrix armMatrix1(armMatrix0 * dYawMatrix(3.141592f));
		armMatrix1.m_posit = armMatrix0.m_posit;
		armMatrix1.m_posit.m_y += 0.4f;
		armMatrix1.m_posit.m_x -= 0.05f;
		armMatrix1.m_posit.m_z -= 0.1f;
		NewtonBody* const armBody1 = CreateBox(scene, armMatrix1, dVector(0.05f, 0.1f, 0.5f));
		dMatrix armHingeMatrix1(dGrammSchmidt(dVector(1.0f, 0.0f, 0.0f)));
		armHingeMatrix1.m_posit = armMatrix1.m_posit + armMatrix1.RotateVector(dVector(0.0f, 0.0f, 0.2f));
		dKukaServoMotor1* const armJoint1 = new dKukaServoMotor1(armHingeMatrix1, armBody1, armBody0);
		void* const armJointNode1 = NewtonInverseDynamicsAddChildNode(m_kinematicSolver, armJointNode0, armJoint1->GetJoint());

		// Robot gripper base
		dMatrix gripperMatrix(dYawMatrix(90.0f * 3.141592f / 180.0f));
		gripperMatrix.m_posit = armMatrix1.m_posit + armMatrix1.m_right.Scale(-0.25f) + gripperMatrix.m_front.Scale(-0.06f);
		NewtonBody* const gripperBase = CreateCylinder(scene, gripperMatrix, 0.1f, -0.15f);
		dMatrix gripperEffectMatrix(dGetIdentityMatrix());
		gripperEffectMatrix.m_up = dVector(1.0f, 0.0f, 0.0f, 0.0f);
		gripperEffectMatrix.m_front = gripperMatrix.m_front;
		gripperEffectMatrix.m_right = gripperEffectMatrix.m_front.CrossProduct(gripperEffectMatrix.m_up);
		gripperEffectMatrix.m_posit = gripperMatrix.m_posit + gripperMatrix.m_front.Scale(0.065f);
		dKukaServoMotor2* const gripperJoint = new dKukaServoMotor2(gripperEffectMatrix, gripperBase, armBody1);
		void* const gripperJointNode = NewtonInverseDynamicsAddChildNode(m_kinematicSolver, armJointNode1, gripperJoint->GetJoint());

		// add the inverse dynamics end effector
		m_effector = new dKukaEndEffector(m_kinematicSolver, gripperJointNode, gripperEffectMatrix, origin);

		NewtonInverseDynamicsEndBuild(m_kinematicSolver);
	}

	void MakeKukaRobot_FD(DemoEntityManager* const scene, const dVector& origin)
	{
		dMatrix location(dRollMatrix(90.0f * 3.141592f / 180.0f));
		location.m_posit = origin;
		location.m_posit.m_y += 0.125f * 0.5f;

		// add Robot Base
		NewtonBody* const parentBody = CreateCylinder(scene, location, 0.35f, 0.125f);
		dMatrix parentMatrix(dGrammSchmidt(dVector(0.0f, 1.0f, 0.0f)));
		parentMatrix.m_posit = location.m_posit;
		dCustomHinge* const fixHinge = new dCustomHinge(parentMatrix, parentBody, NULL);
		fixHinge->EnableLimits(true);
		fixHinge->SetLimits(0.0f, 0.0f);

		// add Robot rotating platform
		dMatrix baseMatrix(dGetIdentityMatrix());
		baseMatrix.m_posit = location.m_posit;
		baseMatrix.m_posit.m_y += 0.125f * 0.5f + 0.11f;
		baseMatrix.m_posit.m_z += 0.125f * 0.5f;
		NewtonBody* const base = CreateBox(scene, baseMatrix, dVector(0.125f, 0.2f, 0.25f));
		dMatrix baseSpin(dGrammSchmidt(dVector(0.0f, 1.0f, 0.0f)));
		baseSpin.m_posit = location.m_posit;
		new dCustomHinge(baseSpin, base, parentBody);

		// add Robot Arm
		dMatrix armMatrix0(dPitchMatrix(45.0f * 3.141592f / 180.0f));
		armMatrix0.m_posit = baseMatrix.m_posit;
		armMatrix0.m_posit.m_y += 0.30f;
		armMatrix0.m_posit.m_x += 0.09f;
		armMatrix0.m_posit.m_z -= 0.125f;
		NewtonBody* const armBody0 = CreateBox(scene, armMatrix0, dVector(0.05f, 0.1f, 0.75f));
		dMatrix armHingeMatrix0(dGrammSchmidt(dVector(1.0f, 0.0f, 0.0f)));
		armHingeMatrix0.m_posit = armMatrix0.m_posit + armMatrix0.RotateVector(dVector(0.0f, 0.0f, 0.3f));
		new dCustomHinge(armHingeMatrix0, armBody0, base);

		dMatrix armMatrix1(armMatrix0 * dYawMatrix(3.141592f));
		armMatrix1.m_posit = armMatrix0.m_posit;
		armMatrix1.m_posit.m_y += 0.4f;
		armMatrix1.m_posit.m_x -= 0.05f;
		armMatrix1.m_posit.m_z -= 0.1f;
		NewtonBody* const armBody1 = CreateBox(scene, armMatrix1, dVector(0.05f, 0.1f, 0.5f));
		dMatrix armHingeMatrix1(dGrammSchmidt(dVector(1.0f, 0.0f, 0.0f)));
		armHingeMatrix1.m_posit = armMatrix1.m_posit + armMatrix1.RotateVector(dVector(0.0f, 0.0f, 0.2f));
		new dCustomHinge(armHingeMatrix1, armBody1, armBody0);

		// Robot gripper base
		dMatrix gripperMatrix(dYawMatrix(90.0f * 3.141592f / 180.0f));
		gripperMatrix.m_posit = armMatrix1.m_posit + armMatrix1.m_right.Scale(-0.25f) + gripperMatrix.m_front.Scale(-0.06f);
		NewtonBody* const gripperBase = CreateCylinder(scene, gripperMatrix, 0.1f, -0.15f);
		dMatrix gripperEffectMatrix(dGetIdentityMatrix());
		gripperEffectMatrix.m_up = dVector(1.0f, 0.0f, 0.0f, 0.0f);
		gripperEffectMatrix.m_front = gripperMatrix.m_front;
		gripperEffectMatrix.m_right = gripperEffectMatrix.m_front.CrossProduct(gripperEffectMatrix.m_up);
		gripperEffectMatrix.m_posit = gripperMatrix.m_posit + gripperMatrix.m_front.Scale(0.065f);
		new dCustomUniversal(gripperEffectMatrix, gripperBase, armBody1);

		// add the inverse dynamics end effector
		m_effector = new dKukaEndEffector(gripperBase, gripperEffectMatrix, origin);

		NewtonInverseDynamicsEndBuild(m_kinematicSolver);
	}

#else

// simplified version for debugging
	void MakeKukaRobot_IK(DemoEntityManager* const scene, const dVector& origin)
	{
		dMatrix location(dRollMatrix(90.0f * 3.141592f / 180.0f));
		location.m_posit = origin;
		location.m_posit.m_y += 0.125f * 0.5f;

		// add Robot Base
		NewtonBody* const parentBody = CreateCylinder(scene, location, 0.35f, 0.125f);
		dMatrix parentMatrix(dGrammSchmidt(dVector(0.0f, 1.0f, 0.0f)));
		parentMatrix.m_posit = location.m_posit;
		NewtonBodySetMassMatrix(parentBody, 0.0f, 0.0f, 0.0f, 0.0f);

		// add Robot rotating platform
		dMatrix baseMatrix(dGetIdentityMatrix());
		baseMatrix.m_posit = location.m_posit;
		baseMatrix.m_posit.m_y += 0.125f * 0.5f + 0.11f;
		baseMatrix.m_posit.m_z += 0.125f * 0.5f;
		NewtonBody* const base = CreateBox(scene, baseMatrix, dVector(0.125f, 0.2f, 0.25f));
		dMatrix baseSpin(dGrammSchmidt(dVector(0.0f, 1.0f, 0.0f)));
		baseSpin.m_posit = location.m_posit;
		dCustomHinge* const fixHinge = new dKukaServoMotor1(baseSpin, base, parentBody);
		fixHinge->EnableLimits(true);
		fixHinge->SetLimits(0.0f, 0.0f);

		void* const baseHingeNode = NewtonInverseDynamicsAddRoot(m_kinematicSolver, base);

		// add Robot Arm
		dMatrix armMatrix0(dPitchMatrix(45.0f * 3.141592f / 180.0f));
		armMatrix0.m_posit = baseMatrix.m_posit;
		armMatrix0.m_posit.m_y += 0.30f;
		armMatrix0.m_posit.m_x += 0.09f;
		armMatrix0.m_posit.m_z -= 0.125f;
		NewtonBody* const armBody0 = CreateBox(scene, armMatrix0, dVector(0.05f, 0.1f, 0.75f));
		dMatrix armHingeMatrix0(dGrammSchmidt(dVector(1.0f, 0.0f, 0.0f)));
		armHingeMatrix0.m_posit = armMatrix0.m_posit + armMatrix0.RotateVector(dVector(0.0f, 0.0f, 0.3f));
		dKukaServoMotor1* const armJoint0 = new dKukaServoMotor1(armHingeMatrix0, armBody0, base);
		void* const armJointNode0 = NewtonInverseDynamicsAddChildNode(m_kinematicSolver, baseHingeNode, armJoint0->GetJoint());

		dMatrix armMatrix1(armMatrix0 * dYawMatrix(3.141592f));
		armMatrix1.m_posit = armMatrix0.m_posit;
		armMatrix1.m_posit.m_y += 0.4f;
		armMatrix1.m_posit.m_x -= 0.05f;
		armMatrix1.m_posit.m_z -= 0.1f;
		NewtonBody* const armBody1 = CreateBox(scene, armMatrix1, dVector(0.05f, 0.1f, 0.5f));
		dMatrix armHingeMatrix1(dGrammSchmidt(dVector(1.0f, 0.0f, 0.0f)));
		armHingeMatrix1.m_posit = armMatrix1.m_posit + armMatrix1.RotateVector(dVector(0.0f, 0.0f, 0.2f));
		dKukaServoMotor1* const armJoint1 = new dKukaServoMotor1(armHingeMatrix1, armBody1, armBody0);
		void* const armJointNode1 = NewtonInverseDynamicsAddChildNode(m_kinematicSolver, armJointNode0, armJoint1->GetJoint());

		// Robot gripper base
		dMatrix gripperMatrix(dYawMatrix(90.0f * 3.141592f / 180.0f));
		gripperMatrix.m_posit = armMatrix1.m_posit + armMatrix1.m_right.Scale(-0.25f) + gripperMatrix.m_front.Scale(-0.06f);
		dMatrix gripperEffectMatrix(dGetIdentityMatrix());
		gripperEffectMatrix.m_up = dVector(1.0f, 0.0f, 0.0f, 0.0f);
		gripperEffectMatrix.m_front = gripperMatrix.m_front;
		gripperEffectMatrix.m_right = gripperEffectMatrix.m_front.CrossProduct(gripperEffectMatrix.m_up);
		gripperEffectMatrix.m_posit = gripperMatrix.m_posit + gripperMatrix.m_front.Scale(0.065f);

		// add the inverse dynamics end effector
		m_effector = new dKukaEndEffector(m_kinematicSolver, armJointNode1, gripperEffectMatrix, origin);

		NewtonInverseDynamicsEndBuild(m_kinematicSolver);
	}

	void MakeKukaRobot_FD(DemoEntityManager* const scene, const dVector& origin)
	{
		dMatrix location(dRollMatrix(90.0f * 3.141592f / 180.0f));
		location.m_posit = origin;
		location.m_posit.m_y += 0.125f * 0.5f;

		// add Robot Base
		NewtonBody* const parentBody = CreateCylinder(scene, location, 0.35f, 0.125f);
		NewtonBodySetMassMatrix(parentBody, 0.0f, 0.0f, 0.0f, 0.0f);
		dMatrix parentMatrix(dGrammSchmidt(dVector(0.0f, 1.0f, 0.0f)));
		parentMatrix.m_posit = location.m_posit;

		// add Robot rotating platform
		dMatrix baseMatrix(dGetIdentityMatrix());
		baseMatrix.m_posit = location.m_posit;
		baseMatrix.m_posit.m_y += 0.125f * 0.5f + 0.11f;
		baseMatrix.m_posit.m_z += 0.125f * 0.5f;
		NewtonBody* const base = CreateBox(scene, baseMatrix, dVector(0.125f, 0.2f, 0.25f));
		dMatrix baseSpin(dGrammSchmidt(dVector(0.0f, 1.0f, 0.0f)));
		baseSpin.m_posit = location.m_posit;
		dCustomHinge* const fixHinge = new dCustomHinge(baseSpin, base, parentBody);
		fixHinge->EnableLimits(true);
		fixHinge->SetLimits(0.0f, 0.0f);


		// add Robot Arm
		dMatrix armMatrix0(dPitchMatrix(45.0f * 3.141592f / 180.0f));
		armMatrix0.m_posit = baseMatrix.m_posit;
		armMatrix0.m_posit.m_y += 0.30f;
		armMatrix0.m_posit.m_x += 0.09f;
		armMatrix0.m_posit.m_z -= 0.125f;
		NewtonBody* const armBody0 = CreateBox(scene, armMatrix0, dVector(0.05f, 0.1f, 0.75f));
		dMatrix armHingeMatrix0(dGrammSchmidt(dVector(1.0f, 0.0f, 0.0f)));
		armHingeMatrix0.m_posit = armMatrix0.m_posit + armMatrix0.RotateVector(dVector(0.0f, 0.0f, 0.3f));
		new dCustomHinge(armHingeMatrix0, armBody0, base);

		dMatrix armMatrix1(armMatrix0 * dYawMatrix(3.141592f));
		armMatrix1.m_posit = armMatrix0.m_posit;
		armMatrix1.m_posit.m_y += 0.4f;
		armMatrix1.m_posit.m_x -= 0.05f;
		armMatrix1.m_posit.m_z -= 0.1f;
		NewtonBody* const armBody1 = CreateBox(scene, armMatrix1, dVector(0.05f, 0.1f, 0.5f));
		dMatrix armHingeMatrix1(dGrammSchmidt(dVector(1.0f, 0.0f, 0.0f)));
		armHingeMatrix1.m_posit = armMatrix1.m_posit + armMatrix1.RotateVector(dVector(0.0f, 0.0f, 0.2f));
		new dCustomHinge(armHingeMatrix1, armBody1, armBody0);

		// Robot gripper base
		dMatrix gripperMatrix(dYawMatrix(90.0f * 3.141592f / 180.0f));
		gripperMatrix.m_posit = armMatrix1.m_posit + armMatrix1.m_right.Scale(-0.25f) + gripperMatrix.m_front.Scale(-0.06f);

		dMatrix gripperEffectMatrix(dGetIdentityMatrix());
		gripperEffectMatrix.m_up = dVector(1.0f, 0.0f, 0.0f, 0.0f);
		gripperEffectMatrix.m_front = gripperMatrix.m_front;
		gripperEffectMatrix.m_right = gripperEffectMatrix.m_front.CrossProduct(gripperEffectMatrix.m_up);
		gripperEffectMatrix.m_posit = gripperMatrix.m_posit + gripperMatrix.m_front.Scale(0.065f);

		// add the inverse dynamics end effector
		m_effector = new dKukaEndEffector(armBody1, gripperEffectMatrix, origin);

		NewtonInverseDynamicsEndBuild(m_kinematicSolver);
	}

#endif


	void Debug(dCustomJoint::dDebugDisplay* const debugContext) const
	{
		m_effector->Debug(debugContext);
	}

	private:
	void ScaleIntertia(NewtonBody* const body, dFloat factor) const
	{
		dFloat Ixx;
		dFloat Iyy;
		dFloat Izz;
		dFloat mass;
		NewtonBodyGetMass(body, &mass, &Ixx, &Iyy, &Izz);
		NewtonBodySetMassMatrix(body, mass, Ixx * factor, Iyy * factor, Izz * factor);
	}

	NewtonBody* CreateBox(DemoEntityManager* const scene, const dMatrix& location, const dVector& size) const
	{
		NewtonWorld* const world = scene->GetNewton();
		int materialID = NewtonMaterialGetDefaultGroupID(world);
		NewtonCollision* const collision = CreateConvexCollision(world, dGetIdentityMatrix(), size, _BOX_PRIMITIVE, 0);
		DemoMesh* const geometry = new DemoMesh("primitive", collision, "smilli.tga", "smilli.tga", "smilli.tga");

		dFloat mass = 1.0f;
		NewtonBody* const body = CreateSimpleSolid(scene, geometry, mass, location, collision, materialID);
		ScaleIntertia(body, 10.0f);

		geometry->Release();
		NewtonDestroyCollision(collision);
		return body;
	}

	NewtonBody* CreateCylinder(DemoEntityManager* const scene, const dMatrix& location, dFloat radius, dFloat height) const
	{
		NewtonWorld* const world = scene->GetNewton();
		int materialID = NewtonMaterialGetDefaultGroupID(world);
		dVector size(radius, height, radius, 0.0f);
		NewtonCollision* const collision = CreateConvexCollision(world, dGetIdentityMatrix(), size, _CYLINDER_PRIMITIVE, 0);
		DemoMesh* const geometry = new DemoMesh("primitive", collision, "smilli.tga", "smilli.tga", "smilli.tga");

		dFloat mass = 1.0f;
		NewtonBody* const body = CreateSimpleSolid(scene, geometry, mass, location, collision, materialID);
		ScaleIntertia(body, 10.0f);

		geometry->Release();
		NewtonDestroyCollision(collision);
		return body;
	}

	void PostUpdate(dFloat timestep, int threadIndex) {}

	void PreUpdate(dFloat timestep, int threadIndex)
	{
		// calculate the current position
		m_effector->SetTarget (m_posit_x, m_posit_y, m_azimuth);
		NewtonInverseDynamicsUpdate(m_kinematicSolver, timestep, threadIndex);
	}

	dKukaEndEffector* m_effector;
	NewtonInverseDynamics* m_kinematicSolver;
	dFloat32 m_azimuth;
	dFloat32 m_posit_x;
	dFloat32 m_posit_y;
};

class dSixAxisManager: public dCustomControllerManager<dSixAxisController>
{
	public:
	dSixAxisManager(DemoEntityManager* const scene)
		:dCustomControllerManager<dSixAxisController>(scene->GetNewton(), "sixAxisManipulator")
		,m_currentController(NULL)
		,m_azimuth(0.0f)
		,m_posit_x(0.0f)
		,m_posit_y(0.0f)
	{
		scene->Set2DDisplayRenderFunction(RenderHelpMenu, NULL, this);
	}

	~dSixAxisManager()
	{
	}

	static void RenderHelpMenu(DemoEntityManager* const scene, void* const context)
	{
		dSixAxisManager* const me = (dSixAxisManager*)context;

		dVector color(1.0f, 1.0f, 0.0f, 0.0f);
		scene->Print(color, "Use sliders to manipulate robot");
		ImGui::SliderFloat("Azimuth", &me->m_azimuth, -360.0f, 360.0f);
		ImGui::SliderFloat("posit_x", &me->m_posit_x, -1.0f, 1.0f);
		ImGui::SliderFloat("posit_y", &me->m_posit_y, -1.0f, 2.0f);

me->m_azimuth = 0.0f;
me->m_posit_y = 2.0f;
		for (dListNode* node = me->GetFirst(); node; node = node->GetNext()) {
			dSixAxisController* const controller = &node->GetInfo();
			controller->SetTarget (me->m_posit_x, me->m_posit_y, me->m_azimuth * 3.141592f / 180.0f);
		}
	}

	virtual dSixAxisController* CreateController()
	{
		dSixAxisController* const controller = (dSixAxisController*)dCustomControllerManager<dSixAxisController>::CreateController();
		controller->Init();
		return controller;
	}

	dSixAxisController* MakeKukaRobot_IK(DemoEntityManager* const scene, const dVector& origin)
	{
		dSixAxisController* const controller = (dSixAxisController*)CreateController();
		controller->MakeKukaRobot_IK(scene, origin);
		m_currentController = controller;
		return controller;
	}

	dSixAxisController* MakeKukaRobot_FD(DemoEntityManager* const scene, const dVector& origin)
	{
		dSixAxisController* const controller = (dSixAxisController*)CreateController();
		controller->MakeKukaRobot_FD(scene, origin);
		m_currentController = controller;
		return controller;
	}


	void OnDebug(dCustomJoint::dDebugDisplay* const debugContext)
	{
		for (dListNode* node = GetFirst(); node; node = node->GetNext()) {
			dSixAxisController* const controller = &node->GetInfo();
			controller->Debug(debugContext);
		}
	}

	dSixAxisController* m_currentController;
	dFloat32 m_azimuth;
	dFloat32 m_posit_x;
	dFloat32 m_posit_y;
};


void SixAxisManipulators(DemoEntityManager* const scene)
{
	// load the sky box
	scene->CreateSkyBox();
	CreateLevelMesh (scene, "flatPlane.ngd", true);
	dSixAxisManager* const robotManager = new dSixAxisManager(scene);

	robotManager->MakeKukaRobot_IK (scene, dVector (0.0f, 0.0f, -0.75f));
	robotManager->MakeKukaRobot_FD (scene, dVector (0.0f, 0.0f,  0.75f));

	dVector origin(0.0f);
	origin.m_x = -2.0f;
	origin.m_y  = 0.5f;
	dQuaternion rot;
	scene->SetCameraMatrix(rot, origin);
}

