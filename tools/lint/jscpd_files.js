// Runs jscpd on a path list read from a JSON file. The full first-party list
// exceeds the Windows command-line limit, so it is passed in memory instead.
// jscpd's config "path" entries cannot carry it: jscpd resolves them to
// backslash paths, which its glob matcher does not match on Windows.
"use strict";

const fs = require("fs");
const path = require("path");

const [listFile, ...options] = process.argv.slice(2);
const files = JSON.parse(fs.readFileSync(listFile, "utf8"));
const executable = path.join(__dirname, "node_modules", "jscpd", "bin", "jscpd");
process.argv = [process.argv[0], executable, ...options, ...files];
require(executable);
