class PorcelainViewer {
    constructor(containerId) {
        this.container = document.getElementById(containerId);
        this.showCracks = true;

        this.model = new PorcelainModel(this.container);
        this.gradient = new CrackGradient({ maxDepth: 200 });
        this.mergedCrackLines = new MergedCrackLines();
        this.mergedCrackTubes = new MergedCrackTubes();

        this.model.startAnimation();
    }

    createPorcelainVase() {
        return this.model.createPorcelainVase();
    }

    loadPorcelain(porcelainData, cracksData = []) {
        this.clearScene();
        this.createPorcelainVase();

        if (cracksData.length > 0) {
            this.loadCracks(cracksData);
        }

        this.model.focusTarget();
    }

    loadCracks(cracksData) {
        this.clearCracks();

        const cracksWithPoints = cracksData.filter(c => {
            const pts = c.points || c.crack_points || [];
            return pts.length >= 2;
        });

        cracksWithPoints.forEach(crack => {
            this.mergedCrackLines.addCrack(crack);
        });

        this.mergedCrackLines.build(this.model.scene);

        if (cracksWithPoints.length <= 100) {
            this.mergedCrackTubes.buildFromCracks(cracksWithPoints, this.model.scene);
        }

        this.mergedCrackLines.setVisible(this.showCracks);
        this.mergedCrackTubes.setVisible(this.showCracks);

        console.log(`[CrackRender] 已加载 ${cracksWithPoints.length} 条裂纹, ` +
                    `顶点=${this.mergedCrackLines.totalVertices}, ` +
                    `线段=${this.mergedCrackLines.totalSegments}, ` +
                    `DrawCall=${this.mergedCrackLines.drawCalls + (this.mergedCrackTubes.mergedMesh ? 1 : 0)}`);
    }

    clearScene() {
        this.model.clearPorcelainMesh();
        this.clearCracks();
    }

    clearCracks() {
        this.mergedCrackLines.clearFromScene(this.model.scene);
        this.mergedCrackLines.clear();
        this.mergedCrackTubes.clearFromScene(this.model.scene);
        this.mergedCrackTubes.clear();
    }

    setShowCracks(show) {
        this.showCracks = show;
        this.mergedCrackLines.setVisible(show);
        this.mergedCrackTubes.setVisible(show);
    }

    setDisplayMode(mode) {
        this.model.setDisplayMode(mode);
    }

    setAutoRotate(rotate) {
        this.model.setAutoRotate(rotate);
    }

    get scene() {
        return this.model.scene;
    }

    get gradient() {
        return this.gradient;
    }

    destroy() {
        this.clearCracks();

        if (this.mergedCrackLines) {
            this.mergedCrackLines.dispose();
        }
        if (this.mergedCrackTubes) {
            this.mergedCrackTubes.dispose();
        }

        this.model.dispose();
    }
}
