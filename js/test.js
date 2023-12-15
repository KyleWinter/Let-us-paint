document.addEventListener('DOMContentLoaded', function() {
	const canvas = document.getElementById('paint-canvas');
	const ctx = canvas.getContext('2d');

	const canvasSize = {
		width: 1000,
		height: 1000
	};
	canvas.width = canvasSize.width;
	canvas.height = canvasSize.height;
	// 初始化画布

	// 定义画布的像素尺寸
	const pixelSize = {
		width: 20,
		height: 20
	}; // 调整为合适的像素尺寸
	const canvasRect = canvas.getBoundingClientRect();

	// 初始化画布
	// 定义颜色数组
	const colors = ['#FF0000', '#00FF00', '#0000FF', '#FFFF00', '#00FFFF', '#FF00FF'];

	const colorCache = new Array(canvasSize.height / pixelSize.height);
	for (let i = 0; i < colorCache.length; i++) {
		colorCache[i] = new Array(canvasSize.width / pixelSize.width);
	}

	for (let y = 0; y < canvasSize.height; y += pixelSize.height) {
		for (let x = 0; x < canvasSize.width; x += pixelSize.width) {
			const colorIndex = (Math.floor(x / pixelSize.width) + Math.floor(y / pixelSize.height)) % colors
				.length;
			const color = colors[colorIndex];
			ctx.fillStyle = color;
			ctx.fillRect(x, y, pixelSize.width, pixelSize.height);
			colorCache[y / pixelSize.height][x / pixelSize.width] = color;
		}
	}

	// 初始选择位置
	let selectedX = 0;
	let selectedY = 0;

	// 处理键盘事件来移动选择
	document.addEventListener('keydown', function(event) {
		if (event.key === 'Space' || event.key === 'Enter') {
			console.log(
				`Focused Pixel: Row ${selectedY / pixelSize.height}, Column ${selectedX / pixelSize.width}`
			);
		}
		switch (event.key) {
			case 'ArrowLeft':
				selectedX = Math.max(selectedX - pixelSize.width, 0);
				break;
			case 'ArrowRight':
				selectedX = Math.min(selectedX + pixelSize.width, canvasSize.width - pixelSize.width);
				break;
			case 'ArrowUp':
				selectedY = Math.max(selectedY - pixelSize.height, 0);
				break;
			case 'ArrowDown':
				selectedY = Math.min(selectedY + pixelSize.height, canvasSize.height - pixelSize
					.height);
				break;
			case 'Space':
				console.log(
					`Focused Pixel: Row ${selectedY / pixelSize.height}, Column ${selectedX / pixelSize.width}`
				);
				break;
			case 'Enter':
				console.log(
					`Focused Pixel: Row ${selectedY / pixelSize.height}, Column ${selectedX / pixelSize.width}`
				);
				break;
		}
		// 更新画布以反映新选择的位置
		updateCanvas(selectedX, selectedY);
	});

	let selectedPixel = {
		x: -1,
		y: -1
	};

	function startBlinking(pixel) {
		let isOriginalColor = true;
		const blinkInterval = setInterval(() => {
			if (pixel.x === selectedX && pixel.y === selectedY) {
				const originalColor = colorCache[pixel.y / pixelSize.height][pixel.x / pixelSize.width];
				let blinkColor = isOriginalColor ? originalColor : (originalColor === '#FFFFFF' ?
					'#000000' : '#FFFFFF');

				ctx.fillStyle = blinkColor;
				ctx.fillRect(pixel.x, pixel.y, pixelSize.width, pixelSize.height);
				isOriginalColor = !isOriginalColor;
			} else {
				clearInterval(blinkInterval); // 停止定时器
				redrawPixel(pixel.x, pixel.y);
			}
		}, 500); // 每秒切换颜色
	}


	function updateCanvas(x, y) {
		console.log(
			`Focused Pixel: Row ${selectedY / pixelSize.height}, Column ${selectedX / pixelSize.width}`
		);
		selectedPixel = {
			x: x,
			y: y
		};
		startBlinking(selectedPixel);
	}

	// 在键盘事件监听中调用 updateCanvas
	document.addEventListener('keydown', function(event) {
		// ...现有的按键逻辑...
		updateCanvas(selectedX, selectedY);
	});

	// 确保在文档加载时初始化闪烁
	updateCanvas(selectedX, selectedY);

	function redrawPixel(x, y) {
		const color = colorCache[y / pixelSize.height][x / pixelSize.width];
		ctx.fillStyle = color;
		ctx.fillRect(x, y, pixelSize.width, pixelSize.height);
	}


	function getPixelColor(x, y) {
		const pixel = ctx.getImageData(x, y, 1, 1).data;
		const color = `rgba(${pixel[0]}, ${pixel[1]}, ${pixel[2]}, ${pixel[3] / 255})`;
		console.log(`Color at (${x}, ${y}): ${color}`);
		return color;
	}

});



