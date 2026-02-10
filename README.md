# Nordic Project Template

This is a template repo and project structure for Nordic Firmware Projects. 

## Configuring Project

### Modifying Project Name
The firmware project lives in the `project` directory of this repo. Feel free to modify that directory name as you see fit. 

**NOTE:** If modifying the `project` directory name, please modify the `g_PROJECT_NAME` variable in `scripts/project.conf` to match the name of your new directory name.

### Setting Nordic SDK & Toolchain Version
In `scripts/project.conf`, modify the `g_NORDIC_TOOLCHAIN_VERSION` to match the toolchain/sdk version you using.

Additionally, make sure that the `revision` property of the `nrf` project under `projects/west.yml` matches the toolchain version specified.

## Initializing Project

Simply run `./scripts/init_project.sh`. This script will automatically handle installing `nrfutil` locally to this repo, along with the toolchain.

After the toolchain is installed, it will run `west init` and `west update`.

You can run this script, even after a project is initialized.  It will simply just run `west update` on the project.

## Building Project

Modify the `g_DEFAULT_BUILD_TARGET` and `g_DEFAULT_BUILD_BOARD` to match the target directory (currently defaulted to `app`) and target board (currently defaulted to `nrf52840dk_nrf52840`) to match the desired defaults.

You can simply run `./scripts/build.sh` to automatically build this for you.

## Dropping Into Toolchain Shell

If you want to directly make `west` calls, you can simply call `toolchain_shell.sh` after the project has been initialized. From here, you can manually call `west build`, etc.


## Static Analysis 
Static analysis through CodeChecker running clang-tidy can be run with `./scripts/run_codechecker.sh` (requires `pip install codechecker`). The script produces a zipped HTML report at `build/analysis/codechecker/codechecker-html.zip`, uses `.clang-tidy` as the default ruleset, `.codechecker.yml` for analyzer options, `.codechecker.tidyargs` for clang-tidy extra args.