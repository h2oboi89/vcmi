/*
* BuyArmyBehavior.h, part of VCMI engine
*
* Authors: listed in file AUTHORS in main folder
*
* License: GNU General Public License v2.0 or later
* Full text of license available in license.txt file, in main folder
*
*/
#pragma once

#include "lib/VCMI_Lib.h"
#include "../Goals/CGoal.h"
#include "../AIUtility.h"

namespace NKAI
{

struct HitMapInfo;

namespace Goals
{

	class DefenceBehavior : public CGoal<DefenceBehavior>
	{
	public:
		DefenceBehavior()
			:CGoal(DEFENCE)
		{
		}

		virtual Goals::TGoalVec decompose() const override;
		virtual std::string toString() const override;

		virtual bool operator==(const DefenceBehavior & other) const override
		{
			return true;
		}

	private:
		void evaluateDefence(Goals::TGoalVec & tasks, const CGTownInstance * town) const;
		void evaluateRecruitingHero(Goals::TGoalVec & tasks, const HitMapInfo & treat, const CGTownInstance * town) const;
	};
}


}
