import { describe, it, expect } from 'vitest';
import fs from 'fs';
import path from 'path';
import { PNG } from 'pngjs';
import pixelmatch from 'pixelmatch';

const RESVG_DIR = path.resolve(__dirname, '../images/png');
const LUNASVG_DIR = path.resolve(__dirname, '../images_output');

function readPNG(filePath: string): PNG {
    const data = fs.readFileSync(filePath);
    return PNG.sync.read(data);
}

describe('Image Comparison (resvg vs lunasvg)', () => {
    const files = fs.readdirSync(RESVG_DIR).filter(f => f.endsWith('.png'));

    files.forEach(file => {
        it(`should have minimal difference for ${file}`, () => {
            const resvgPath = path.join(RESVG_DIR, file);
            const lunasvgPath = path.join(LUNASVG_DIR, file);

            expect(fs.existsSync(lunasvgPath), `Output file missing: ${file}`).toBe(true);

            const img1 = readPNG(resvgPath);
            const img2 = readPNG(lunasvgPath);

            expect(img1.width).toBe(img2.width);
            expect(img1.height).toBe(img2.height);

            const { width, height } = img1;
            const diff = new PNG({ width, height });

            const numDiffPixels = pixelmatch(
                img1.data,
                img2.data,
                diff.data,
                width,
                height,
                { threshold: 0.1 } // Sensitivity threshold
            );

            const totalPixels = width * height;
            const diffPercentage = (numDiffPixels / totalPixels) * 100;

            console.log(`${file}: Diff = ${diffPercentage.toFixed(2)}% (${numDiffPixels} pixels)`);

            // Assert that the difference is less than 1% (adjust as needed)
            expect(diffPercentage).toBeLessThan(1.0);
        });
    });
});
