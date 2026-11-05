#include "EnginePCH.h"
#include "Sequencer.h"
#include "Sequence/MovieSceneFadeTrack.h"
#include "Sequence/MovieSceneFadeSection.h"
#include <algorithm>
#include <cmath>

UMovieSceneFadeTrack* FLevelSequencePanel::FindFadeTrack() const
{
	if (MovieScene)
	{
		for (UMovieScenePropertyTrack* Track : MovieScene->GetTracks())
		{
			if (UMovieSceneFadeTrack* FadeTrack = Cast<UMovieSceneFadeTrack>(Track))
			{
				return FadeTrack;
			}
		}
	}
	return nullptr;
}

void FLevelSequencePanel::AddFadeTrack()
{
	if (!MovieScene || FindFadeTrack())
	{
		return;
	}
	UMovieSceneFadeTrack* FadeTrack = NewObject<UMovieSceneFadeTrack>(MovieScene);
	UMovieSceneFadeSection* FadeSection = Cast<UMovieSceneFadeSection>(FadeTrack->AddSection());
	FadeSection->SetRange({ 0.f, static_cast<float>(MovieScene->GetFrameCount() + 1) });
	MovieScene->AddTrack(FadeTrack);
	SelectFadeSection(FadeSection);
	bSequenceDirty = true;
}

void FLevelSequencePanel::SelectFadeSection(UMovieSceneFadeSection* FadeSection)
{
	SelectedFadeSection = FadeSection;
	SelectedCameraCut = nullptr;
	SelectedActorName.clear();
	Section = nullptr;
	Track = nullptr;
	SelectedKeyChannel = -1;
	DragChannel = -1;
	bFadeKeySelected = false;
	bDraggingFadeKey = false;
	FadeKeyValue = 0.f;
	if (FadeSection)
	{
		FadeSection->EvaluateFadeCurve(FFrameTime::FromDecimal(static_cast<float>(PreviewFrame)), FadeKeyValue);
	}
}

bool FLevelSequencePanel::GetFadePreview(float& OutAmount, FLinearColor& OutColor) const
{
	if (!IsOpen() || !MovieScene)
	{
		return false;
	}
	bool bHasFade = false;
	for (UMovieScenePropertyTrack* Track : MovieScene->GetTracks())
	{
		if (UMovieSceneFadeTrack* FadeTrack = Cast<UMovieSceneFadeTrack>(Track))
		{
			UMovieSceneFadeSection* FadeSection = Cast<UMovieSceneFadeSection>(FadeTrack->FindSection(PreviewFrame));
			float Amount = 0.f;
			if (FadeSection && FadeSection->EvaluateFadeCurve(FFrameTime::FromDecimal(static_cast<float>(PreviewFrame)), Amount))
			{
				OutAmount = std::clamp(Amount, 0.f, 1.f);
				OutColor = FadeSection->GetFadeColor();
				bHasFade = true;
			}
		}
	}
	return bHasFade;
}

void FLevelSequencePanel::DrawFadeTimeline()
{
	for (UMovieScenePropertyTrack* Track : MovieScene->GetTracks())
	{
		UMovieSceneFadeTrack* FadeTrack = Cast<UMovieSceneFadeTrack>(Track);
		if (!FadeTrack)
		{
			continue;
		}
		ImGui::PushID(FadeTrack);
		if (ImGui::BeginNeoTimelineEx("Fade"))
		{
			const bool bSelectTrack = ImGui::IsItemClicked(ImGuiMouseButton_Left) || ImGui::IsItemClicked(ImGuiMouseButton_Right);
			for (UMovieSceneSection* Section : FadeTrack->GetAllSections())
			{
				UMovieSceneFadeSection* FadeSection = Cast<UMovieSceneFadeSection>(Section);
				if (!FadeSection)
				{
					continue;
				}
				const FFloatRange Range = FadeSection->GetRange();
				int32_t Start = static_cast<int32_t>(Range.Min);
				int32_t End = static_cast<int32_t>(Range.Max);
				ImGui::PushID(FadeSection);
				if (ImGui::NeoSection("##FadeSection", &Start, &End, "Fade", SelectedFadeSection == FadeSection, 0, MovieScene->GetFrameCount() + 1) || bSelectTrack)
				{
					SelectFadeSection(FadeSection);
				}
				if (Start != Range.Min || End != Range.Max)
				{
					FadeSection->SetRange({ static_cast<float>(Start), static_cast<float>(End) });
					bSequenceDirty = true;
				}
				ImGui::PopID();
			}
			ImGui::EndNeoTimeLine();
		}
		for (UMovieSceneSection* Section : FadeTrack->GetAllSections())
		{
			UMovieSceneFadeSection* FadeSection = Cast<UMovieSceneFadeSection>(Section);
			if (!FadeSection)
			{
				continue;
			}
			ImGui::PushID(FadeSection);
			FMovieSceneFloatChannel& Curve = FadeSection->GetFadeCurve();
			if (ImGui::BeginNeoTimelineEx("Fade amount"))
			{
				if (bDraggingFadeKey && SelectedFadeSection == FadeSection && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
				{
					const FFloatRange Range = FadeSection->GetRange();
					const float Offset = (ImGui::GetMousePos().x - FadeDragMouseX) / FadeDragFrameWidth;
					FadeDragTargetFrame = static_cast<int32>(std::clamp(std::round(FadeKeyFrame + static_cast<double>(Offset)), static_cast<double>(Range.Min), static_cast<double>(Range.Max - 1)));
				}
				const auto& Times = Curve.GetTimes();
				const auto& Values = Curve.GetValues();
				for (int32 i = 0; i < Times.Num(); i++)
				{
					const bool bSelected = SelectedFadeSection == FadeSection && bFadeKeySelected && FadeKeyFrame == Times[i];
					int32_t Frame = bSelected && bDraggingFadeKey ? FadeDragTargetFrame : Times[i];
					ImGui::PushID(i);
					if (bSelected)
					{
						ImGui::PushNeoSequencerStyleColor(ImGuiNeoSequencerCol_Keyframe, IM_COL32(255, 190, 60, 255));
					}
					ImGui::NeoKeyframe(&Frame);
					if (ImGui::IsNeoKeyframeHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
					{
						SelectFadeSection(FadeSection);
						bFadeKeySelected = true;
						FadeKeyFrame = Times[i];
						FadeKeyValue = Values[i].Value;
						bDraggingFadeKey = true;
						FadeDragTargetFrame = FadeKeyFrame;
						FadeDragMouseX = ImGui::GetMousePos().x;
						FadeDragFrameWidth = (std::max)(ImGui::NeoGetFrameWidth(), 0.001f);
						if (SequencePlayer)
						{
							SequencePlayer->Stop();
							bFollowPlayerTime = false;
						}
					}
					if (bSelected)
					{
						ImGui::PopNeoSequencerStyleColor();
					}
					ImGui::PopID();
				}
				ImGui::EndNeoTimeLine();
			}
			if (bDraggingFadeKey && SelectedFadeSection == FadeSection && !ImGui::IsMouseDown(ImGuiMouseButton_Left))
			{
				bool bOccupied = false;
				for (int32 Frame : Curve.GetTimes())
				{
					bOccupied |= Frame == FadeDragTargetFrame;
				}
				if (!bOccupied && FadeDragTargetFrame != FadeKeyFrame)
				{
					Curve.MoveKey(FadeKeyFrame, FadeDragTargetFrame);
					FadeKeyFrame = FadeDragTargetFrame;
					bSequenceDirty = true;
				}
				bDraggingFadeKey = false;
			}
			ImGui::PopID();
		}
		ImGui::PopID();
	}
}

void FLevelSequencePanel::DrawFadeEditor()
{
	if (!SelectedFadeSection)
	{
		return;
	}
	ImGui::Separator();
	ImGui::TextUnformatted("Fade");
	ImGui::PushID("FadeEditor");
	ImGui::BeginDisabled(bDraggingFadeKey);
	FMovieSceneFloatChannel& Curve = SelectedFadeSection->GetFadeCurve();
	int KeyIndex = -1;
	if (bFadeKeySelected)
	{
		for (int32 i = 0; i < Curve.GetTimes().Num(); i++)
		{
			if (Curve.GetTimes()[i] == FadeKeyFrame)
			{
				KeyIndex = i;
				FadeKeyValue = Curve.GetValues()[i].Value;
				break;
			}
		}
	}
	const bool bValueChanged = ImGui::SliderFloat("Amount", &FadeKeyValue, 0.f, 1.f);
	int Interpolation = KeyIndex >= 0 ? static_cast<int>(Curve.GetValues()[KeyIndex].InterpMode) : 1;
	bool bInterpolationChanged = false;
	if (KeyIndex >= 0)
	{
		bInterpolationChanged |= ImGui::RadioButton("Constant", &Interpolation, 0);
		ImGui::SameLine();
		bInterpolationChanged |= ImGui::RadioButton("Linear", &Interpolation, 1);
	}
	if (KeyIndex >= 0 && (bValueChanged || bInterpolationChanged))
	{
		Curve.UpdateOrAddKey(FadeKeyFrame, { static_cast<ECurveInterpMode>(Interpolation), FadeKeyValue });
		bSequenceDirty = true;
	}
	const FFloatRange Range = SelectedFadeSection->GetRange();
	ImGui::BeginDisabled(PreviewFrame < Range.Min || PreviewFrame >= Range.Max);
	if (ImGui::Button("Add key at cursor"))
	{
		Curve.UpdateOrAddKey(PreviewFrame, { ECurveInterpMode::Linear, FadeKeyValue });
		FadeKeyFrame = PreviewFrame;
		bFadeKeySelected = true;
		bSequenceDirty = true;
	}
	ImGui::EndDisabled();
	ImGui::SameLine();
	ImGui::BeginDisabled(KeyIndex < 0);
	if (ImGui::Button("Delete key"))
	{
		Curve.RemoveKey(FadeKeyFrame);
		bFadeKeySelected = false;
		bSequenceDirty = true;
	}
	ImGui::EndDisabled();
	FLinearColor Color = SelectedFadeSection->GetFadeColor();
	float RGB[] = { Color.R, Color.G, Color.B };
	if (ImGui::ColorEdit3("Fade color", RGB))
	{
		SelectedFadeSection->SetFadeColor(FLinearColor(RGB[0], RGB[1], RGB[2], Color.A));
		bSequenceDirty = true;
	}
	float Amount = 0.f;
	if (SelectedFadeSection->EvaluateFadeCurve(FFrameTime::FromDecimal(static_cast<float>(PreviewFrame)), Amount))
	{
		ImGui::Text("Frame %d | Fade %.3f", PreviewFrame, Amount);
	}
	ImGui::EndDisabled();
	ImGui::PopID();
}
