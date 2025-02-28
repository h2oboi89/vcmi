/*
 * JsonRandom.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#include "StdInc.h"
#include "JsonRandom.h"

#include <vstd/StringUtils.h>

#include "JsonNode.h"
#include "CRandomGenerator.h"
#include "constants/StringConstants.h"
#include "VCMI_Lib.h"
#include "CArtHandler.h"
#include "CCreatureHandler.h"
#include "CCreatureSet.h"
#include "spells/CSpellHandler.h"
#include "CSkillHandler.h"
#include "CHeroHandler.h"
#include "IGameCallback.h"
#include "mapObjects/IObjectInterface.h"
#include "modding/IdentifierStorage.h"
#include "modding/ModScope.h"

VCMI_LIB_NAMESPACE_BEGIN

namespace JsonRandom
{
	si32 loadValue(const JsonNode & value, CRandomGenerator & rng, si32 defaultValue)
	{
		if(value.isNull())
			return defaultValue;
		if(value.isNumber())
			return static_cast<si32>(value.Float());
		if(value.isVector())
		{
			const auto & vector = value.Vector();

			size_t index= rng.getIntRange(0, vector.size()-1)();
			return loadValue(vector[index], rng, 0);
		}
		if(value.isStruct())
		{
			if (!value["amount"].isNull())
				return static_cast<si32>(loadValue(value["amount"], rng, defaultValue));
			si32 min = static_cast<si32>(loadValue(value["min"], rng, 0));
			si32 max = static_cast<si32>(loadValue(value["max"], rng, 0));
			return rng.getIntRange(min, max)();
		}
		return defaultValue;
	}

	std::string loadKey(const JsonNode & value, CRandomGenerator & rng, const std::set<std::string> & valuesSet)
	{
		if(value.isString())
			return value.String();
		
		if(value.isStruct())
		{
			if(!value["type"].isNull())
				return value["type"].String();

			if(!value["anyOf"].isNull())
				return RandomGeneratorUtil::nextItem(value["anyOf"].Vector(), rng)->String();
			
			if(!value["noneOf"].isNull())
			{
				auto copyValuesSet = valuesSet;
				for(auto & s : value["noneOf"].Vector())
					copyValuesSet.erase(s.String());
				
				if(!copyValuesSet.empty())
					return *RandomGeneratorUtil::nextItem(copyValuesSet, rng);
			}
		}
		
		return valuesSet.empty() ? "" : *RandomGeneratorUtil::nextItem(valuesSet, rng);
	}

	TResources loadResources(const JsonNode & value, CRandomGenerator & rng)
	{
		TResources ret;

		if (value.isVector())
		{
			for (const auto & entry : value.Vector())
				ret += loadResource(entry, rng);
			return ret;
		}

		for (size_t i=0; i<GameConstants::RESOURCE_QUANTITY; i++)
		{
			ret[i] = loadValue(value[GameConstants::RESOURCE_NAMES[i]], rng);
		}
		return ret;
	}

	TResources loadResource(const JsonNode & value, CRandomGenerator & rng)
	{
		std::set<std::string> defaultResources(std::begin(GameConstants::RESOURCE_NAMES), std::end(GameConstants::RESOURCE_NAMES) - 1); //except mithril
		
		std::string resourceName = loadKey(value, rng, defaultResources);
		si32 resourceAmount = loadValue(value, rng, 0);
		si32 resourceID(VLC->identifiers()->getIdentifier(value.meta, "resource", resourceName).value());

		TResources ret;
		ret[resourceID] = resourceAmount;
		return ret;
	}


	std::vector<si32> loadPrimary(const JsonNode & value, CRandomGenerator & rng)
	{
		std::vector<si32> ret;
		if(value.isStruct())
		{
			for(const auto & name : NPrimarySkill::names)
			{
				ret.push_back(loadValue(value[name], rng));
			}
		}
		if(value.isVector())
		{
			ret.resize(GameConstants::PRIMARY_SKILLS, 0);
			std::set<std::string> defaultStats(std::begin(NPrimarySkill::names), std::end(NPrimarySkill::names));
			for(const auto & element : value.Vector())
			{
				auto key = loadKey(element, rng, defaultStats);
				defaultStats.erase(key);
				int id = vstd::find_pos(NPrimarySkill::names, key);
				if(id != -1)
					ret[id] += loadValue(element, rng);
			}
		}
		return ret;
	}

	std::map<SecondarySkill, si32> loadSecondary(const JsonNode & value, CRandomGenerator & rng)
	{
		std::map<SecondarySkill, si32> ret;
		if(value.isStruct())
		{
			for(const auto & pair : value.Struct())
			{
				SecondarySkill id(VLC->identifiers()->getIdentifier(pair.second.meta, "skill", pair.first).value());
				ret[id] = loadValue(pair.second, rng);
			}
		}
		if(value.isVector())
		{
			std::set<std::string> defaultSkills;
			for(const auto & skill : VLC->skillh->objects)
			{
				IObjectInterface::cb->isAllowed(2, skill->getIndex());
				auto scopeAndName = vstd::splitStringToPair(skill->getJsonKey(), ':');
				if(scopeAndName.first == ModScope::scopeBuiltin() || scopeAndName.first == value.meta)
					defaultSkills.insert(scopeAndName.second);
				else
					defaultSkills.insert(skill->getJsonKey());
			}
			
			for(const auto & element : value.Vector())
			{
				auto key = loadKey(element, rng, defaultSkills);
				defaultSkills.erase(key); //avoid dupicates
				if(auto identifier = VLC->identifiers()->getIdentifier(ModScope::scopeGame(), "skill", key))
				{
					SecondarySkill id(identifier.value());
					ret[id] = loadValue(element, rng);
				}
			}
		}
		return ret;
	}

	ArtifactID loadArtifact(const JsonNode & value, CRandomGenerator & rng)
	{
		if (value.getType() == JsonNode::JsonType::DATA_STRING)
			return ArtifactID(VLC->identifiers()->getIdentifier("artifact", value).value());

		std::set<CArtifact::EartClass> allowedClasses;
		std::set<ArtifactPosition> allowedPositions;
		ui32 minValue = 0;
		ui32 maxValue = std::numeric_limits<ui32>::max();

		if (value["class"].getType() == JsonNode::JsonType::DATA_STRING)
			allowedClasses.insert(CArtHandler::stringToClass(value["class"].String()));
		else
			for(const auto & entry : value["class"].Vector())
				allowedClasses.insert(CArtHandler::stringToClass(entry.String()));

		if (value["slot"].getType() == JsonNode::JsonType::DATA_STRING)
			allowedPositions.insert(ArtifactPosition::decode(value["class"].String()));
		else
			for(const auto & entry : value["slot"].Vector())
				allowedPositions.insert(ArtifactPosition::decode(entry.String()));

		if (!value["minValue"].isNull()) minValue = static_cast<ui32>(value["minValue"].Float());
		if (!value["maxValue"].isNull()) maxValue = static_cast<ui32>(value["maxValue"].Float());

		return VLC->arth->pickRandomArtifact(rng, [=](const ArtifactID & artID) -> bool
		{
			CArtifact * art = VLC->arth->objects[artID];

			if(!vstd::iswithin(art->getPrice(), minValue, maxValue))
				return false;

			if(!allowedClasses.empty() && !allowedClasses.count(art->aClass))
				return false;
			
			if(!IObjectInterface::cb->isAllowed(1, art->getIndex()))
				return false;

			if(!allowedPositions.empty())
			{
				for(const auto & pos : art->getPossibleSlots().at(ArtBearer::HERO))
				{
					if(allowedPositions.count(pos))
						return true;
				}
				return false;
			}
			return true;
		});
	}

	std::vector<ArtifactID> loadArtifacts(const JsonNode & value, CRandomGenerator & rng)
	{
		std::vector<ArtifactID> ret;
		for (const JsonNode & entry : value.Vector())
		{
			ret.push_back(loadArtifact(entry, rng));
		}
		return ret;
	}

	SpellID loadSpell(const JsonNode & value, CRandomGenerator & rng, std::vector<SpellID> spells)
	{
		if (value.getType() == JsonNode::JsonType::DATA_STRING)
			return SpellID(VLC->identifiers()->getIdentifier("spell", value).value());

		if (!value["level"].isNull())
		{
			int32_t spellLevel = value["level"].Float();

			vstd::erase_if(spells, [=](const SpellID & spell)
			{
				return VLC->spellh->getById(spell)->getLevel() != spellLevel;
			});
		}

		if (!value["school"].isNull())
		{
			int32_t schoolID = VLC->identifiers()->getIdentifier("spellSchool", value["school"]).value();

			vstd::erase_if(spells, [=](const SpellID & spell)
			{
				return !VLC->spellh->getById(spell)->hasSchool(SpellSchool(schoolID));
			});
		}

		if (spells.empty())
		{
			logMod->warn("Failed to select suitable random spell!");
			return SpellID::NONE;
		}
		return SpellID(*RandomGeneratorUtil::nextItem(spells, rng));
	}

	std::vector<SpellID> loadSpells(const JsonNode & value, CRandomGenerator & rng, const std::vector<SpellID> & spells)
	{
		std::vector<SpellID> ret;
		for (const JsonNode & entry : value.Vector())
		{
			ret.push_back(loadSpell(entry, rng, spells));
		}
		return ret;
	}

	std::vector<PlayerColor> loadColors(const JsonNode & value, CRandomGenerator & rng)
	{
		std::vector<PlayerColor> ret;
		std::set<std::string> def;
		
		for(auto & color : GameConstants::PLAYER_COLOR_NAMES)
			def.insert(color);
		
		for(auto & entry : value.Vector())
		{
			auto key = loadKey(entry, rng, def);
			auto pos = vstd::find_pos(GameConstants::PLAYER_COLOR_NAMES, key);
			if(pos < 0)
				logMod->warn("Unable to determine player color %s", key);
			else
				ret.emplace_back(pos);
		}
		return ret;
	}

	std::vector<HeroTypeID> loadHeroes(const JsonNode & value, CRandomGenerator & rng)
	{
		std::vector<HeroTypeID> ret;
		for(auto & entry : value.Vector())
		{
			ret.push_back(VLC->heroTypes()->getByIndex(VLC->identifiers()->getIdentifier("hero", entry.String()).value())->getId());
		}
		return ret;
	}

	std::vector<HeroClassID> loadHeroClasses(const JsonNode & value, CRandomGenerator & rng)
	{
		std::vector<HeroClassID> ret;
		for(auto & entry : value.Vector())
		{
			ret.push_back(VLC->heroClasses()->getByIndex(VLC->identifiers()->getIdentifier("heroClass", entry.String()).value())->getId());
		}
		return ret;
	}

	CStackBasicDescriptor loadCreature(const JsonNode & value, CRandomGenerator & rng)
	{
		CStackBasicDescriptor stack;
		stack.type = VLC->creh->objects[VLC->identifiers()->getIdentifier("creature", value["type"]).value()];
		stack.count = loadValue(value, rng);
		if (!value["upgradeChance"].isNull() && !stack.type->upgrades.empty())
		{
			if (int(value["upgradeChance"].Float()) > rng.nextInt(99)) // select random upgrade
			{
				stack.type = VLC->creh->objects[*RandomGeneratorUtil::nextItem(stack.type->upgrades, rng)];
			}
		}
		return stack;
	}

	std::vector<CStackBasicDescriptor> loadCreatures(const JsonNode & value, CRandomGenerator & rng)
	{
		std::vector<CStackBasicDescriptor> ret;
		for (const JsonNode & node : value.Vector())
		{
			ret.push_back(loadCreature(node, rng));
		}
		return ret;
	}

	std::vector<RandomStackInfo> evaluateCreatures(const JsonNode & value)
	{
		std::vector<RandomStackInfo> ret;
		for (const JsonNode & node : value.Vector())
		{
			RandomStackInfo info;

			if (!node["amount"].isNull())
				info.minAmount = info.maxAmount = static_cast<si32>(node["amount"].Float());
			else
			{
				info.minAmount = static_cast<si32>(node["min"].Float());
				info.maxAmount = static_cast<si32>(node["max"].Float());
			}
			const CCreature * crea = VLC->creh->objects[VLC->identifiers()->getIdentifier("creature", node["type"]).value()];
			info.allowedCreatures.push_back(crea);
			if (node["upgradeChance"].Float() > 0)
			{
				for(const auto & creaID : crea->upgrades)
					info.allowedCreatures.push_back(VLC->creh->objects[creaID]);
			}
			ret.push_back(info);
		}
		return ret;
	}

	//std::vector<Component> loadComponents(const JsonNode & value)
	//{
	//	std::vector<Component> ret;
	//	return ret;
	//	//TODO
	//}

	std::vector<Bonus> DLL_LINKAGE loadBonuses(const JsonNode & value)
	{
		std::vector<Bonus> ret;
		for (const JsonNode & entry : value.Vector())
		{
			if(auto bonus = JsonUtils::parseBonus(entry))
				ret.push_back(*bonus);
		}
		return ret;
	}

}

VCMI_LIB_NAMESPACE_END
