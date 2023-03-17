#include "assets/asset_loader.hh"

#include "base/debug.hh"
#include "base/filesystem.hh"

static String AssetLoader_DataDirectory = String();

void InitAssetLoader() {
	if (AssetLoader_DataDirectory) { return; }

	if (PLATFORM_WEB) {
		AssetLoader_DataDirectory = "data";
	} else {
		// Search for the asset directory above the given start_dir.
		auto find_data_directory = [](String start_dir) {
			// Should restore the initial working directory before returning
			String initial_wd = GetCurrentDir();

			bool found = PathIsDirectory("data");
			while (!found) {
				if (!SetCurrentDir("..")) {
					SetCurrentDir(initial_wd);
					return String();
				}
				found = PathIsDirectory("data");
			}

			// FIXME: Should implement a GetAbsolutePath() to simplify this
			SetCurrentDir("data");
			String directory = GetCurrentDir();

			SetCurrentDir(initial_wd);
			return directory;
		};

		if (!AssetLoader_DataDirectory) {
			// Check above the current working directory
			String maybe_asset_dir = find_data_directory(GetCurrentDir());
			if (maybe_asset_dir) {
				AssetLoader_DataDirectory = std::move(maybe_asset_dir);
			}
		}

		#if 0 // GetExecutableDir not implemented
		if (!AssetLoader_DataDirectory) {
			// Check above the executable directory
			String maybe_asset_dir = find_data_directory(GetExecutableDir());
			if (maybe_asset_dir) {
				AssetLoader_DataDirectory = std::move(maybe_asset_dir);
			}
		}
		#endif

		if (!AssetLoader_DataDirectory) {
			Panic("Failed to locate data directory");
		}
	}
	LOG_F(INFO, "Data directory: %s", AssetLoader_DataDirectory.cstr);

	InitTextureLoader();
	InitShaderLoader();
	InitModelLoader();
}

uint32_t ProcessAssetLoadOperation() {
	return 0;
}
