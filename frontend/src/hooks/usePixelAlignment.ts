import { useLayoutEffect, useRef, type DependencyList } from 'react';

/** Aligns the positioned preview without compositor transforms. */
export function usePixelAlignment<T extends HTMLElement>(dependencies: DependencyList) {
    const ref = useRef<T>(null);

    useLayoutEffect(() => {
        const element = ref.current;
        if (!element) return;
        const originalLeft = element.style.left;
        const originalTop = element.style.top;
        let disposed = false;
        let frame = 0;

        const align = () => {
            if (disposed) return;
            // Measure the natural layout each time, so corrections never accumulate.
            element.style.left = originalLeft;
            element.style.top = originalTop;
            const computed = getComputedStyle(element);
            const baseLeft = computed.left === 'auto' ? '0px' : computed.left;
            const baseTop = computed.top === 'auto' ? '0px' : computed.top;
            const anchor = element.querySelector('.skill-level') ?? element;
            const { x, y } = anchor.getBoundingClientRect();
            const dpr = window.devicePixelRatio || 1;
            const dx = Math.round(x * dpr) / dpr - x;
            const dy = Math.round(y * dpr) / dpr - y;
            element.style.left = `calc(${baseLeft} + ${dx}px)`;
            element.style.top = `calc(${baseTop} + ${dy}px)`;
        };
        const schedule = () => {
            cancelAnimationFrame(frame);
            frame = requestAnimationFrame(align);
        };

        // Run before paint, then repeat as fonts or the surrounding layout change.
        align();
        const observer = new ResizeObserver(schedule);
        for (let ancestor: Element | null = element; ancestor; ancestor = ancestor.parentElement) {
            observer.observe(ancestor);
        }
        window.addEventListener('resize', schedule);
        window.addEventListener('scroll', schedule, true);
        window.visualViewport?.addEventListener('resize', schedule);
        window.visualViewport?.addEventListener('scroll', schedule);
        document.fonts.addEventListener('loadingdone', schedule);
        void document.fonts.ready.then(() => { if (!disposed) schedule(); });

        // Re-arm when moving between displays or changing browser zoom/DPR.
        let resolution: MediaQueryList;
        const watchResolution = () => {
            resolution?.removeEventListener('change', onResolutionChange);
            resolution = matchMedia(`(resolution: ${window.devicePixelRatio}dppx)`);
            resolution.addEventListener('change', onResolutionChange);
        };
        function onResolutionChange() { watchResolution(); schedule(); }
        watchResolution();

        return () => {
            disposed = true;
            cancelAnimationFrame(frame);
            observer.disconnect();
            window.removeEventListener('resize', schedule);
            window.removeEventListener('scroll', schedule, true);
            window.visualViewport?.removeEventListener('resize', schedule);
            window.visualViewport?.removeEventListener('scroll', schedule);
            document.fonts.removeEventListener('loadingdone', schedule);
            resolution.removeEventListener('change', onResolutionChange);
            element.style.left = originalLeft;
            element.style.top = originalTop;
        };
    }, dependencies);

    return ref;
}
