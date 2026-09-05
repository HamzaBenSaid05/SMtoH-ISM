// Fill out your copyright notice in the Description page of Project Settings.

#include "SMConverterStyle.h"

#include "Interfaces/IPluginManager.h"
#include "Styling/SlateStyleRegistry.h"
#include "Brushes/SlateImageBrush.h"
#include "Misc/Paths.h"

TSharedPtr<FSlateStyleSet> SMConverterStyle::StyleSet;

void SMConverterStyle::Initialize()
{
	if (StyleSet.IsValid())
	{
		return;
	}

	StyleSet = MakeShared<FSlateStyleSet>("SMConverterStyle");

	const FString ResourcesPath =
		IPluginManager::Get().FindPlugin(TEXT("SMConverter"))
			->GetBaseDir() / TEXT("Resources");

	StyleSet->SetContentRoot(ResourcesPath);

	StyleSet->Set(
		"SMConverter.SMConverter",
		new FSlateVectorImageBrush(
			StyleSet->RootToContentDir(TEXT("SMConverter"), TEXT(".svg")),
			FVector2D(40.0f, 40.0f)
		)
	);

	FSlateStyleRegistry::RegisterSlateStyle(*StyleSet);
}

void SMConverterStyle::Shutdown()
{
	if (StyleSet.IsValid())
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*StyleSet);
		StyleSet.Reset();
	}
}

const ISlateStyle& SMConverterStyle::Get()
{
	return *StyleSet;
}

FName SMConverterStyle::GetStyleSetName()
{
	return "SMConverterStyle";
}
