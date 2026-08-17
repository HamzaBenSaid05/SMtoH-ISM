// Copyright Epic Games, Inc. All Rights Reserved.

#include "SMConverter.h"

#include "SMtoISMConverter.h"
#include "Modules/ModuleManager.h"
#include "ToolMenus.h"
#include "ToolMenu.h"
#include "Textures/SlateIcon.h"

#define LOCTEXT_NAMESPACE "FSMConverterModule"

void FSMConverterModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	UToolMenus::RegisterStartupCallback(
										FSimpleMulticastDelegate::FDelegate::CreateRaw(
																					   this,
																					   &FSMConverterModule::RegisterMenus
																					  )
									   );   
}

void FSMConverterModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
	UToolMenus::UnregisterOwner(this);
}

void FSMConverterModule::RegisterMenus()
{
	FToolMenuOwnerScoped Owner(this);

    UToolMenu* Menu =
        UToolMenus::Get()->ExtendMenu("LevelEditor.ActorContextMenu");

    FToolMenuSection& Section =
        Menu->FindOrAddSection("ActorOptions");

    Section.AddSubMenu(
        "SMToInstances",
    FText::FromString("SM to Instances"),                           
        FText::FromString("Convert selected Static Mesh Actors to instances"),
        FNewToolMenuDelegate::CreateLambda(
            [](UToolMenu* SubMenu)
            {
                FToolMenuSection& Section =
                    SubMenu->FindOrAddSection("Conversions");

                Section.AddMenuEntry(
                    "ConvertToISM",
                    FText::FromString("Convert to ISM"),
                    FText::FromString(
                        "Convert selected Static Mesh Actors to Instanced Static Mesh"
                    ),
                    FSlateIcon(),
                    FToolMenuExecuteAction::CreateLambda(
                        [](const FToolMenuContext&)
                        {
                            FSMtoISMSettings Settings;

                            Settings.bUseHISM = false;
                            Settings.bReadFromSource = true;
                            Settings.bDeleteSourceActors = true;

                            FSMtoISMConverter::Convert(Settings);
                        }
                    )
                );

                Section.AddMenuEntry(
                    "ConvertToHISM",
                    FText::FromString("Convert to HISM"),
                    FText::FromString(
                        "Convert selected Static Mesh Actors to Hierarchical Instanced Static Mesh"
                    ),
                    FSlateIcon(),
                    FToolMenuExecuteAction::CreateLambda(
                        [](const FToolMenuContext&)
                        {
                            FSMtoISMSettings Settings;

                            Settings.bUseHISM = true;
                            Settings.bReadFromSource = true;
                            Settings.bDeleteSourceActors = true;

                            FSMtoISMConverter::Convert(Settings);
                        }
                    )
                );

                Section.AddMenuEntry(
                    "ResetToStaticMesh",
                    FText::FromString("Reset To Static Mesh"),
                    FText::FromString(
                        "Reset Selected SM or ISM actors and reconvert them to static mesh actors"
                    ),
                    FSlateIcon(),
                    FToolMenuExecuteAction::CreateLambda(
                        [](const FToolMenuContext&)
                        {
                            constexpr FISMtoSMSettings Settings;
                            FSMtoISMConverter::ConvertBack(Settings);
                        }
                    )
                );
            }
        )
    );
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FSMConverterModule, SMConverter)