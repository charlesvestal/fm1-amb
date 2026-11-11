// made by tools/build.py: the emulator's parts (fm1.toml emu) and the app's own part of the page
import { compose } from "./parts/parts.js";
const own = {};
export const { attach, message } = compose({  }, own, { id: "amb", name: "AMB", backupExt: "bak" });
