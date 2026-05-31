<template>
  <div class="pixel-cat-wrapper">
    <img :src="catSrc" :width="size" :height="size" :style="catStyle" />
  </div>
</template>

<script>
export default {
  name: 'PixelCat',
  props: {
    mood: { type: String, default: 'neutral' },
    size: { type: Number, default: 64 }
  },
  computed: {
    catSrc() {
      return '/cat.png'
    },
    catStyle() {
      return {
        borderRadius: '50%',
        objectFit: 'cover'
      }
    }
  }
}
</script>

<style scoped>
.pixel-cat-wrapper { display: inline-block; }
.pixel-cat-wrapper img { display: block; }
</style>