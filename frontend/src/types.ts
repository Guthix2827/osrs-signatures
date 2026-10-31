export type Background = 'blue' | 'green' | 'red';
export type Pose = 'none' | 'knight' | 'wizard';

export interface CardConfig {
    name: string;
    design?: string;
    background: Background;
    pose: Pose;
}

export interface GeneratedSignature {
    imageUrl: string;
    shareUrl: string;
    downloadUrl?: string;
}

export interface SignatureProps {
    config: CardConfig;
    requestId: number;
    onGenerated: (signature: GeneratedSignature) => void;
    onError: (message: string) => void;
}
