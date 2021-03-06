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

// dCustomHinge.h: interface for the dCustomHinge class.
//
//////////////////////////////////////////////////////////////////////


#ifndef _CUSTOMHINGE_H_
#define _CUSTOMHINGE_H_

#include "dCustom6dof.h"

class dCustomHinge: public dCustom6dof
{
	public:
	CUSTOM_JOINTS_API dCustomHinge (const dMatrix& pinAndPivotFrame, NewtonBody* const child, NewtonBody* const parent = NULL);
	CUSTOM_JOINTS_API dCustomHinge (const dMatrix& pinAndPivotFrameChild, const dMatrix& pinAndPivotFrameParent, NewtonBody* const child, NewtonBody* const parent = NULL);

	CUSTOM_JOINTS_API virtual ~dCustomHinge();

	CUSTOM_JOINTS_API void EnableLimits(bool state);
	CUSTOM_JOINTS_API void SetLimits(dFloat minAngle, dFloat maxAngle);
	CUSTOM_JOINTS_API dFloat GetJointAngle () const;
	CUSTOM_JOINTS_API dVector GetPinAxis () const;
	CUSTOM_JOINTS_API dFloat GetJointOmega () const;

	CUSTOM_JOINTS_API void SetAsSpringDamper(bool state, dFloat springDamperRelaxation, dFloat spring, dFloat damper);

	CUSTOM_JOINTS_API void SetFriction (dFloat frictionTorque);
	CUSTOM_JOINTS_API dFloat GetFriction () const;

	protected:
	CUSTOM_JOINTS_API virtual void Deserialize (NewtonDeserializeCallback callback, void* const userData); 
	CUSTOM_JOINTS_API virtual void Serialize (NewtonSerializeCallback callback, void* const userData) const; 
	CUSTOM_JOINTS_API virtual void SubmitConstraintsFreeDof(int freeDof, const dMatrix& matrix0, const dMatrix& matrix1, dFloat timestep, int threadIndex);

	private:
	void SubmitConstraintsLimitsOnly(const dMatrix& matrix0, const dMatrix& matrix1, dFloat timestep);
	void SubmitConstraintsFrictionOnly(const dMatrix& matrix0, const dMatrix& matrix1, dFloat timestep);
	void SubmitConstraintsFrictionAndLimit(const dMatrix& matrix0, const dMatrix& matrix1, dFloat timestep);

	dFloat m_minAngle;
	dFloat m_maxAngle;
	dFloat m_friction;
	dFloat m_jointOmega;

	dFloat m_spring;
	dFloat m_damper;
	dFloat m_springDamperRelaxation;
	union
	{
		int m_flags;
		struct
		{
			unsigned m_limitsOn			 : 1;
			unsigned m_setAsSpringDamper : 1;
			unsigned m_actuatorFlag		 : 1;
			unsigned m_lastRowWasUsed	 : 1;
		};
	};

	DECLARE_CUSTOM_JOINT(dCustomHinge, dCustom6dof)
};

#endif 

