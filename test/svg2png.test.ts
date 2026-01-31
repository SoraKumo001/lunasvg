import { describe, it, expect, afterAll } from "vitest";
import { execFile } from "child_process";
import path from "path";
import fs from "fs";
import { promisify } from "util";

const execFileAsync = promisify(execFile);

const fileExists = async (filePath: string): Promise<boolean> => {
  try {
    await fs.promises.access(filePath);
    return true;
  } catch {
    return false;
  }
};

// Possible paths for the binary depending on build config
const BINARY_PATHS: string[] = [
  path.resolve(__dirname, "../build/examples/Debug/svg2png.exe"),
  path.resolve(__dirname, "../build/examples/Release/svg2png.exe"),
  path.resolve(__dirname, "../build/examples/svg2png.exe"),
  // For non-Windows or other layouts
  path.resolve(__dirname, "../build/examples/svg2png"),
];

const getBinaryPath = async (): Promise<string | null> => {
  for (const p of BINARY_PATHS) {
    if (await fileExists(p)) return p;
  }
  return null;
};

describe("svg2png", () => {
  const assetsDir = path.resolve(__dirname, "assets");
  const inputSvg = "filter.svg"; // Relative to assetsDir
  const outputPng = path.resolve(assetsDir, "rect.svg.png");

  afterAll(async () => {
    // Cleanup
    // if (await fileExists(outputPng)) {
    //   await fs.promises.unlink(outputPng);
    // }
  });

  it("should convert svg to png", async () => {
    const binaryPath = await getBinaryPath();
    if (!binaryPath) {
      console.warn(
        "Skipping test: svg2png binary not found. Please build the project first (e.g., cmake build).",
      );
      throw new Error(
        `svg2png binary not found in expected paths: ${BINARY_PATHS.join(", ")}`,
      );
    }

    // Ensure input exists
    const inputPath = path.resolve(assetsDir, inputSvg);
    expect(await fileExists(inputPath)).toBe(true);

    // Run svg2png
    // We set cwd to assetsDir so the output file is written there
    await execFileAsync(binaryPath, [inputSvg], {
      cwd: assetsDir,
    });

    // Check if output exists
    const exists = await fileExists(outputPng);
    expect(exists).toBe(true);
  });
});
