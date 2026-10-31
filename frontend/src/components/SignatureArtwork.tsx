import type { Ref } from 'react';
import { combatLevel, type PlayerStats } from '../api';
import type { CardConfig } from '../types';
import '../signature.css';

import attackIcon from '../osrs-skill-icons/attack.png';
import strengthIcon from '../osrs-skill-icons/strength.png';
import defenceIcon from '../osrs-skill-icons/defence.png';
import rangedIcon from '../osrs-skill-icons/ranged.png';
import prayerIcon from '../osrs-skill-icons/prayer.png';
import hitpointsIcon from '../osrs-skill-icons/hitpoints.png';
import agilityIcon from '../osrs-skill-icons/agility.png';
import herbloreIcon from '../osrs-skill-icons/herblore.png';
import thievingIcon from '../osrs-skill-icons/thieving.png';
import craftingIcon from '../osrs-skill-icons/crafting.png';
import miningIcon from '../osrs-skill-icons/mining.png';
import smithingIcon from '../osrs-skill-icons/smithing.png';
import fishingIcon from '../osrs-skill-icons/fishing.png';
import cookingIcon from '../osrs-skill-icons/cooking.png';
import firemakingIcon from '../osrs-skill-icons/firemaking.png';
import magicIcon from '../osrs-skill-icons/magic.png';
import fletchingIcon from '../osrs-skill-icons/fletching.png';
import woodcuttingIcon from '../osrs-skill-icons/woodcutting.png';
import runecraftIcon from '../osrs-skill-icons/runecraft.png';
import slayerIcon from '../osrs-skill-icons/slayer.png';
import farmingIcon from '../osrs-skill-icons/farming.png';
import constructionIcon from '../osrs-skill-icons/construction.png';
import hunterIcon from '../osrs-skill-icons/hunter.png';
import sailingIcon from '../osrs-skill-icons/sailing.png';

export const sampleSkills = [
    ['Attack', 99], ['Strength', 99], ['Defence', 99], ['Ranged', 99], ['Prayer', 80],
    ['Hitpoints', 99], ['Agility', 85], ['Herblore', 90], ['Thieving', 81], ['Crafting', 88],
    ['Mining', 87], ['Smithing', 82], ['Fishing', 92], ['Cooking', 99], ['Firemaking', 99],
    ['Magic', 99], ['Fletching', 91], ['Woodcutting', 94], ['Runecraft', 77], ['Slayer', 93],
    ['Farming', 96], ['Construction', 84], ['Hunter', 89], ['Sailing', 72],
] as const;
// Icons are bundled from src/osrs-skill-icons by Vite.
const skillIcons: Record<string, string> = {
    attack: attackIcon,
    strength: strengthIcon,
    defence: defenceIcon,
    ranged: rangedIcon,
    prayer: prayerIcon,
    hitpoints: hitpointsIcon,
    agility: agilityIcon,
    herblore: herbloreIcon,
    thieving: thievingIcon,
    crafting: craftingIcon,
    mining: miningIcon,
    smithing: smithingIcon,
    fishing: fishingIcon,
    cooking: cookingIcon,
    firemaking: firemakingIcon,
    magic: magicIcon,
    fletching: fletchingIcon,
    woodcutting: woodcuttingIcon,
    runecraft: runecraftIcon,
    slayer: slayerIcon,
    farming: farmingIcon,
    construction: constructionIcon,
    hunter: hunterIcon,
    sailing: sailingIcon,
};

interface SignatureArtworkProps {
    config: CardConfig;
    player: PlayerStats | null;
    ref?: Ref<HTMLDivElement>;
    calculatedCombat?: number | null;
}

function ShadowText({ children }: { children: string }) {
    return (
        <span className="shadow-text" data-text={children}>
            {children}
        </span>
    );
}

// Shared by the editable Dev tab and the server's PNG rendering page.
export default function SignatureArtwork({ config, player, ref, calculatedCombat }: SignatureArtworkProps) {
    return (
        <div ref={ref} data-design={config.design ?? 'classic'} className={`signature ${config.background} ${config.pose !== 'none' ? 'with-pose' : ''}`} role="img"
                aria-label={`${player ? 'Hiscores' : 'Sample'} signature for ${config.name}`}>
        <div className="sig-name">
            <ShadowText>
                {config.name}
            </ShadowText>
        </div>
        <div className="skill-grid">
            {sampleSkills.map(([name, sampleLevel]) => {
                const level = player ? player.skills[name.toLowerCase()]?.level ?? '--' : sampleLevel;
                return (
                <div className="skill" key={name} title={`${name}: ${level}`}>
                    <img
                        src={skillIcons[name.toLowerCase()]}
                        alt=""
                        aria-hidden="true"
                        width={16}
                        height={16}
                        draggable={false}
                        style={{
                            width: 16,
                            height: 16,
                            flexShrink: 0,
                            objectFit: 'contain',
                            imageRendering: 'pixelated',
                        }}
                    />
                    <span className="skill-level" data-level={level}>{level}</span>
                </div>
            );})}
        </div>
        {config.pose !== 'none' && <svg className={`character ${config.pose}`} viewBox="0 0 40 85" aria-hidden="true"
                                        shapeRendering="crispEdges">{config.pose === 'knight' ? <>
            <path d="M13 3H27V19H13ZM10 22H30L34 51H27V79H21V56H18V79H11V51H6Z" fill="#a5aeb5" stroke="#303943"
                  strokeWidth="2"/>
            <path d="M14 9H27V13H14ZM16 23H24V48H16Z" fill="#3e5266"/>
            <path d="M2 26 11 29 10 52 3 47Z" fill="#687d94" stroke="#263c51"/>
            <path d="M35 12V52M31 36H39" stroke="#e0d7bb" strokeWidth="3"/>
        </> : <>
            <path d="M9 22 20 2 29 22ZM12 27H28L35 78H5Z" fill="#405cb4" stroke="#263465" strokeWidth="2"/>
            <path d="M15 22H25V32H15Z" fill="#d5b58b"/>
            <path d="M34 19V80" stroke="#aa8652" strokeWidth="3"/>
            <path d="M30 15 34 10 39 15 34 22Z" fill="#b0e9ed"/>
        </>}</svg>}
        <div className="sig-summary">
            <ShadowText>
                {`Combat: ${player
                    ? (calculatedCombat === undefined
                    ? combatLevel(player)
                    : calculatedCombat) ?? '--'
                    : 123}`}
            </ShadowText>

            <ShadowText>
                {`Total: ${player ? player.totalLevel.toLocaleString('en-US') : '2,173'}`}
            </ShadowText>
        </div>
    </div>
    );
}
