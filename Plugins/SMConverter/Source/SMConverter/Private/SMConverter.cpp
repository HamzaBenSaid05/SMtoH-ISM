// Copyright Epic Games, Inc. All Rights Reserved.

#include "SMConverter.h"

#include "SMtoISMConverter.h"
#include "Modules/ModuleManager.h"
#include "ToolMenus.h"
#include "ToolMenu.h"
#include "Textures/SlateIcon.h"
#include "SMConverterStyle.h"

#define LOCTEXT_NAMESPACE "FSMConverterModule"

void FSMConverterModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module

	SMConverterStyle::Initialize();

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
	                   LOCTEXT("SMtoInstances", "SM to Instances"),
	                   LOCTEXT(
	                           "SMtoInstancesTooltip",
	                           "Convert selected Static Mesh Actors to instances"
	                          ),
	                   FNewToolMenuDelegate::CreateLambda(
	                                                      [](UToolMenu* SubMenu)
	                                                      {
		                                                      FToolMenuSection& Section =
			                                                      SubMenu->FindOrAddSection("Conversions");

		                                                      Section.AddMenuEntry(
		                                                                           "ConvertToISM",
		                                                                           LOCTEXT("ConvertToISM", "Convert to ISM"),
		                                                                           LOCTEXT(
		                                                                                   "ConvertToISMTooltip",
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
		                                                                           LOCTEXT("ConvertToHISM", "Convert to HISM"),
		                                                                           LOCTEXT(
		                                                                                   "ConvertToHISMTooltip",
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
		                                                                           LOCTEXT("ResetToStaticMesh", "Reset To Static Mesh"),
		                                                                           LOCTEXT("ResetToStaticMeshTooltip",
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
	                                                     ),
	                   false,
	                   FSlateIcon(
	                              SMConverterStyle::GetStyleSetName(),
	                              "SMConverter.SMConverter"
	                             )
	                  );
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FSMConverterModule, SMConverter)
