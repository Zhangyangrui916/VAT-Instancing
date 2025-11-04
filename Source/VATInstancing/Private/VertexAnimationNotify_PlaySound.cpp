#include "VertexAnimationNotify_PlaySound.h"
#include "VATInstancedProxyComponent.h"
#include "Audio.h"
#include "Sound/SoundBase.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

void UVertexAnimationNotify_PlaySound::VertexAnimationNotify(UVATInstancedProxyComponent* Proxy)
{
	IVertexAnimationNotifyInterface::VertexAnimationNotify(Proxy);

	if (Sound)
	{
		if (!Sound->IsOneShot())
		{
			UE_LOG(LogAudio, Warning, TEXT("PlaySound notify: tried to play a sound asset which is not a one-shot: '%s'. Spawning suppressed."),  *GetNameSafe(Sound));
			return;
		}

#if WITH_EDITORONLY_DATA
		UWorld* World = Proxy->GetWorld();
		if (bPreviewIgnoreAttenuation && World && World->WorldType == EWorldType::EditorPreview)
		{
			UGameplayStatics::PlaySound2D(World, Sound, VolumeMultiplier, PitchMultiplier);
		}
		else
#endif
		{
			if (bFollow)
			{
				//TODO: Proxy don't have socket... should use VertexAnimSkeleton
				UGameplayStatics::SpawnSoundAttached(Sound, Proxy, AttachName, FVector(ForceInit), EAttachLocation::SnapToTarget, false, VolumeMultiplier, PitchMultiplier);
			}
			else
			{
				UGameplayStatics::PlaySoundAtLocation(Proxy->GetWorld(), Sound, Proxy->GetComponentLocation(), VolumeMultiplier, PitchMultiplier);
			}
		}
	}
}