new Vue({
	el: '#app',
	data() {
		return {
			selectedPixel: {
				x: -1,
				y: -1
			},
			players: [{
					name: '玩家1',
					blockCount: 15,
					lastDrawTime: '10:45 AM'
				},
				{
					name: '玩家2',
					blockCount: 20,
					lastDrawTime: '10:45 AM'
				},
				{
					name: '玩家3',
					blockCount: 21,
					lastDrawTime: '10:45 AM'
				},
				{
					name: '玩家4',
					blockCount: 43,
					lastDrawTime: '10:45 AM'
				},
				{
					name: '玩家5',
					blockCount: 15,
					lastDrawTime: '10:45 AM'
				},
				{
					name: '玩家6',
					blockCount: 20,
					lastDrawTime: '10:45 AM'
				},
				{
					name: '玩家7',
					blockCount: 21,
					lastDrawTime: '10:45 AM'
				},
				{
					name: '玩家8',
					blockCount: 43,
					lastDrawTime: '10:45 AM'
				},
				{
					name: '玩家9',
					blockCount: 15,
					lastDrawTime: '10:45 AM'
				},
				{
					name: '玩家10',
					blockCount: 20,
					lastDrawTime: '10:45 AM'
				},
				{
					name: '玩家11',
					blockCount: 21,
					lastDrawTime: '10:45 AM'
				},
				{
					name: '玩家12',
					blockCount: 43,
					lastDrawTime: '10:45 AM'
				},
				// Add more test players as needed
			],
			selectedColorIndex: -1, // 初始化选中的颜色索引
			colors: this.generateColorPalette(), // 生成色盘
			selectedX: 0,
			selectedY: 0,
			canvasSize: {
				width: 1000,
				height: 1000
			},
			pixelSize: {
				width: 10,
				height: 10
			},
			colorCache: [],
			ctx: null,
		};
	},

	mounted() {
		this.initializeCanvas();
		window.addEventListener('keydown', this.handleKeyDown);
	},

	beforeDestroy() {
		window.removeEventListener('keydown', this.handleKeyDown);
	},

	methods: {
		decodeColor(index) {
			// 将索引分解为三个通道
			let red = (index & 0b110000) >> 4;
			let green = (index & 0b001100) >> 2;
			let blue = index & 0b000011;

			// 将每个通道的值转换为颜色值（0, 85, 170, 255）
			let levels = [0, 85, 170, 255];

			// 将颜色值转换为16进制字符串
			let redHex = levels[red].toString(16).padStart(2, '0');
			let greenHex = levels[green].toString(16).padStart(2, '0');
			let blueHex = levels[blue].toString(16).padStart(2, '0');

			return `#${redHex}${greenHex}${blueHex}`;
		},

		initializeCanvas() {
			const canvas = this.$refs.paintCanvas;
			this.ctx = canvas.getContext('2d');
			canvas.width = this.canvasSize.width; // 设置画布的实际宽度
			canvas.height = this.canvasSize.height; // 设置画布的实际高度
			this.setupColorCache();
			this.drawCanvas();
		},

		setupColorCache() {
			for (let i = 0; i < this.canvasSize.height / this.pixelSize.height; i++) {
				this.colorCache[i] = new Array(this.canvasSize.width / this.pixelSize.width);
			}
		},
		drawCanvas() {
			// 创建一个 100x100 的二维数组
			for (let y = 0; y < 100; y++) {
				this.colorCache[y] = [];
				for (let x = 0; x < 100; x++) {
					// 为每个元素生成随机颜色索引
					const colorIndex = Math.floor(Math.random() * 64);
					// 使用解码函数获取颜色
					const color = this.decodeColor(colorIndex);
					// 将颜色填充到画布上
					this.ctx.fillStyle = color;
					this.ctx.fillRect(x * this.pixelSize.width, y * this.pixelSize.height, this.pixelSize.width,
						this.pixelSize.height);
					// 保存颜色到缓存中
					this.colorCache[y][x] = color;
				}
			}
		},



		handleKeyDown(event) {
			if (['Space', 'Enter'].includes(event.key)) {
				console.log(
					`Focused Pixel: Row ${this.selectedY / this.pixelSize.height}, Column ${this.selectedX / this.pixelSize.width}`
				);
				return;
			}
			switch (event.key) {
				case 'ArrowLeft':
					this.selectedX = Math.max(this.selectedX - this.pixelSize.width, 0);
					break;
				case 'ArrowRight':
					this.selectedX = Math.min(this.selectedX + this.pixelSize.width, this.canvasSize.width -
						this.pixelSize.width);
					break;
				case 'ArrowUp':
					this.selectedY = Math.max(this.selectedY - this.pixelSize.height, 0);
					break;
				case 'ArrowDown':
					this.selectedY = Math.min(this.selectedY + this.pixelSize.height, this.canvasSize.height -
						this.pixelSize.height);
					break;
				case 'a':
					this.selectedX = Math.max(this.selectedX - this.pixelSize.width, 0);
					break;
				case 'd':
					this.selectedX = Math.min(this.selectedX + this.pixelSize.width, this.canvasSize.width -
						this.pixelSize.width);
					break;
				case 'w':
					this.selectedY = Math.max(this.selectedY - this.pixelSize.height, 0);
					break;
				case 's':
					this.selectedY = Math.min(this.selectedY + this.pixelSize.height, this.canvasSize.height -
						this.pixelSize.height);
					break;
				case 'Enter':
					console.log(
						`Enter Row ${this.selectedY / this.pixelSize.height}, Column ${this.selectedX / this.pixelSize.width}`
					);
				case 'Space':
					console.log(
						`Space Row ${this.selectedY / this.pixelSize.height}, Column ${this.selectedX / this.pixelSize.width}`
					);
					break;
			}
			console.log(
				`Focused Pixel: Row ${this.selectedY / this.pixelSize.height}, Column ${this.selectedX / this.pixelSize.width}`
			);
			this.updateCanvas(this.selectedX, this.selectedY);
		},
		updateCanvas(x, y) {
			if (this.selectedPixel.x !== -1 && this.selectedPixel.y !== -1) {
				// 重绘上一个像素点的原始颜色
				this.redrawPixel(this.selectedPixel.x, this.selectedPixel.y);
			}
			// 更新当前选中的像素点
			this.selectedPixel = {
				x,
				y
			};
			// 获取当前像素点的颜色
			const currentColor = this.colorCache[y / this.pixelSize.height][x / this.pixelSize.width];
			// 设置视觉暂留的颜色（如果原色为白，则为黑色，否则为白色）
			const tempColor = currentColor === '#FFFFFF' ? '#000000' : '#FFFFFF';
			// 立即绘制视觉暂留颜色
			this.ctx.fillStyle = tempColor;
			this.ctx.fillRect(x, y, this.pixelSize.width, this.pixelSize.height);
			// 设置定时器1秒后重绘为原来的颜色
			setTimeout(() => {
				this.ctx.fillStyle = currentColor;
				this.ctx.fillRect(x, y, this.pixelSize.width, this.pixelSize.height);
				this.colorCache[y / this.pixelSize.height][x / this.pixelSize.width] = currentColor;
			}, 1000);
			// 开始闪烁效果
			this.startBlinking(this.selectedPixel);
		},

		startBlinking(pixel) {
			clearInterval(this.blinkInterval); // Clear any existing interval
			this.blinkInterval = setInterval(() => {
				if (pixel.x === this.selectedX && pixel.y === this.selectedY) {
					const originalColor = this.colorCache[pixel.y / this.pixelSize.height][pixel.x /
						this.pixelSize.width
					];
					const blinkColor = this.isOriginalColor ? originalColor : (originalColor ===
						'#FFFFFF' ? '#000000' : '#FFFFFF');
					this.ctx.fillStyle = blinkColor;
					this.ctx.fillRect(pixel.x, pixel.y, this.pixelSize.width, this.pixelSize.height);
					this.isOriginalColor = !this.isOriginalColor;
				} else {
					clearInterval(this.blinkInterval); // Stop blinking if pixel focus has changed
					this.redrawPixel(pixel.x, pixel.y);
				}
			}, 300);
		},

		redrawPixel(x, y) {
			const color = this.colorCache[y / this.pixelSize.height][x / this.pixelSize.width];
			this.ctx.fillStyle = color;
			this.ctx.fillRect(x, y, this.pixelSize.width, this.pixelSize.height);
		},

		generateColorPalette() {
			let palette = [];
			for (let i = 0; i < 64; i++) {
				palette.push(this.decodeColor(i)); // 使用解码函数生成颜色
			}
			return palette;
		},

		selectColor(index) {
			this.selectedColorIndex = index;
			console.log('Selected color index:', index);
		},
	},

});